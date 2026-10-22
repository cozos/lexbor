/*
 * Mirrors the lexbor calls resiliparse's extract_plain_text(main_content=True)
 * makes per document: parse, two CSS selector queries, then a pre-order walk
 * doing the attribute lookups of _is_main_content_node and text_content for
 * link-cluster checks.
 *
 * usage: harness <corpus.bin> [reps] [mode]
 *   mode: all (default) | parse | verify (serialize DOM + results, print hash)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#include <lexbor/html/html.h>
#include <lexbor/css/css.h>
#include <lexbor/selectors/selectors.h>

typedef struct { uint8_t *data; size_t len; } doc_t;

static uint64_t fnv(uint64_t h, const void *p, size_t n) {
    const uint8_t *b = p;
    for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 0x100000001b3ULL; }
    return h;
}

static double now(void) {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static lxb_status_t sel_cb(lxb_dom_node_t *node, lxb_css_selector_specificity_t spec, void *ctx) {
    lxb_dom_collection_append(ctx, node);
    return LXB_STATUS_OK;
}

static const char *MAIN_SEL = ".article-body, .articleBody, .contentBody, .article-text,"
    ".main-content, .postcontent, .post-content, .single-post,[role=\"main\"]";
static const char *SKIP_SEL = "script,style,iframe,frame,template,noscript,textarea,input,button,select,option,label";

static const char *ATTRS[] = {"hidden", "rel", "itemprop", "aria-hidden", "aria-expanded",
                              "class", "id", "style", "data-ad", "data-advertisement",
                              "data-text-ad", "role"};

static lxb_dom_collection_t *query(lxb_css_parser_t *parser, lxb_dom_node_t *root,
                                   const char *sel, size_t init) {
    lxb_selectors_t *s = lxb_selectors_create();
    lxb_selectors_init(s);
    lxb_css_selectors_t *css_sel = lxb_css_selectors_create();
    lxb_css_selectors_init(css_sel);
    lxb_css_parser_selectors_set(parser, css_sel);
    lxb_css_selector_list_t *list = lxb_css_selectors_parse(parser, (const lxb_char_t *) sel, strlen(sel));
    if (parser->status != LXB_STATUS_OK) { fprintf(stderr, "css parse error\n"); exit(1); }
    lxb_dom_collection_t *coll = lxb_dom_collection_make(root->owner_document, init);
    lxb_selectors_find(s, root, list, (lxb_selectors_cb_f) sel_cb, coll);
    lxb_css_selectors_destroy(css_sel, true);
    lxb_css_parser_selectors_set(parser, NULL);
    lxb_selectors_destroy(s, true);
    return coll;
}

static lxb_dom_node_t *next_node(lxb_dom_node_t *root, lxb_dom_node_t *node, int *end) {
    if (!*end && node->first_child) return node->first_child;
    while (!node->next && node != root) { node = node->parent; *end = 1; return node; }
    *end = 0;
    if (node == root) return NULL;
    return node->next;
}

int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "rb");
    int reps = argc > 2 ? atoi(argv[2]) : 1;
    const char *mode = argc > 3 ? argv[3] : "all";
    int verify = !strcmp(mode, "verify"), parse_only = !strcmp(mode, "parse");
    fseek(f, 0, SEEK_END); size_t fsz = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc(fsz); if (fread(buf, 1, fsz, f) != fsz) return 1; fclose(f);
    size_t ndocs = 0, cap = 1024; doc_t *docs = malloc(cap * sizeof(doc_t));
    for (size_t off = 0; off < fsz;) {
        uint32_t n; memcpy(&n, buf + off, 4); off += 4;
        if (ndocs == cap) { cap *= 2; docs = realloc(docs, cap * sizeof(doc_t)); }
        docs[ndocs].data = buf + off; docs[ndocs].len = n; ndocs++; off += n;
    }

    double best = 1e9;
    uint64_t h = 0;
    for (int r = 0; r < reps; r++) {
        h = 0xcbf29ce484222325ULL;
        double t0 = now(), tparse = 0;
        for (size_t i = 0; i < ndocs; i++) {
            double tp = now();
            lxb_html_document_t *doc = lxb_html_document_create();
            lxb_html_document_parse(doc, docs[i].data, docs[i].len);
            tparse += now() - tp;
            if (verify) {
                lexbor_str_t str = {0};
                lxb_html_serialize_tree_str(lxb_dom_interface_node(doc), &str);
                h = fnv(h, str.data, str.length);
            }
            if (parse_only || !doc->body) { lxb_html_document_destroy(doc); continue; }

            lxb_css_memory_t *mem = lxb_css_memory_create();
            lxb_css_memory_init(mem, 128);
            lxb_css_parser_t *parser = lxb_css_parser_create();
            lxb_css_parser_init(parser, NULL);
            lxb_css_parser_memory_set(parser, mem);

            lxb_dom_node_t *root = lxb_dom_interface_node(doc->body);
            lxb_dom_collection_t *c = query(parser, root, MAIN_SEL, 5);
            h = fnv(h, &c->array.length, sizeof(size_t));
            if (lxb_dom_collection_length(c) == 1) root = lxb_dom_collection_node(c, 0);
            lxb_dom_collection_destroy(c, true);

            c = query(parser, root, SKIP_SEL, 30);
            size_t nskip = lxb_dom_collection_length(c);
            h = fnv(h, &nskip, sizeof nskip);
            for (size_t k = 0; k < nskip; k++) {
                uintptr_t ln = lxb_dom_collection_node(c, k)->local_name;
                h = fnv(h, &ln, sizeof ln);
            }
            lxb_dom_collection_destroy(c, true);

            int end = 0;
            for (lxb_dom_node_t *node = root; node; node = next_node(root, node, &end)) {
                if (end || node->type != LXB_DOM_NODE_TYPE_ELEMENT) continue;
                lxb_dom_element_t *el = lxb_dom_interface_element(node);
                for (size_t a = 0; a < sizeof ATTRS / sizeof *ATTRS; a++) {
                    size_t vlen = 0;
                    const lxb_char_t *v = lxb_dom_element_get_attribute(el, (const lxb_char_t *) ATTRS[a],
                                                                        strlen(ATTRS[a]), &vlen);
                    if (v) { h = fnv(h, &a, sizeof a); h = fnv(h, v, vlen); }
                }
                if (node->local_name == LXB_TAG_UL || node->local_name == LXB_TAG_FOOTER) {
                    size_t tl = 0;
                    lxb_char_t *t = lxb_dom_node_text_content(node, &tl);
                    h = fnv(h, &tl, sizeof tl);
                    if (t) lxb_dom_document_destroy_text(node->owner_document, t);
                }
            }
            lxb_css_parser_destroy(parser, true);
            lxb_css_memory_destroy(mem, true);
            lxb_html_document_destroy(doc);
        }
        double el = now() - t0;
        if (el < best) best = el;
        fprintf(stderr, "rep %d: %.3fs (parse %.3fs)\n", r, el, tparse);
    }
    printf("docs=%zu best=%.3fs hash=%016llx\n", ndocs, best, (unsigned long long) h);
    return 0;
}
