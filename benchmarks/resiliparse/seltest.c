/* Differential test: hash of (preorder index, specificity) callbacks for many selector lists. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <lexbor/html/html.h>
#include <lexbor/css/css.h>
#include <lexbor/selectors/selectors.h>

static uint64_t H;
static void mix(uint64_t v) { H ^= v; H *= 0x100000001b3ULL; }
static lxb_dom_node_t *ROOT;

static size_t preorder_index(lxb_dom_node_t *n) {
    size_t i = 0; lxb_dom_node_t *x = ROOT;
    while (x) { if (x == n) return i; i++;
        if (x->first_child) x = x->first_child;
        else { while (x && !x->next) x = x->parent; if (x) x = x->next; } }
    return (size_t) -1;
}
static int stop_after;
static lxb_status_t cb(lxb_dom_node_t *node, lxb_css_selector_specificity_t spec, void *ctx) {
    mix(preorder_index(node)); mix(spec); (*(size_t *) ctx)++;
    if (stop_after && *(size_t *) ctx >= (size_t) stop_after) return LXB_STATUS_STOP;
    return LXB_STATUS_OK;
}

static const char *SELS[] = {
    "script,style,iframe,frame,template,noscript,textarea,input,button,select,option,label",
    ".article-body, .articleBody, .contentBody, .article-text,.main-content, .postcontent, .post-content, .single-post,[role=\"main\"]",
    "div", "DIV, Span, a", "*", "a, a, a", ".a, .A, .b", "#main, #MAIN, .x", "#content", "notatag, alsonot",
    "[class]", "[id], [data-foo]", "[href^=http]", "[href$=\".html\"]", "[href*=google]", "[lang|=en]", "[class~=nav]",
    "[rel=nofollow i]", "[type=text], input", ".nav,.menu,.footer,.header,.content,.post,.entry,.sidebar,.widget,.btn",
    "p,div,span,a,li,ul,img,h1,h2,h3,table,tr,td,section,article,header,footer,nav,main,aside",
    "svg, path, rect, circle, g, math", "[viewbox], [viewBox]", "body, html, head",
    ".row, [role=navigation], div, #footer, *",
    "amp-img, ytd-app, my-element, custom-tag, wix-image", "[DATA-ID], [Data-Src], [aria-label]", "a, amp-img, [data-src], .x, .y",
};

int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); size_t fsz = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc(fsz); if (fread(buf, 1, fsz, f) != fsz) return 1;
    H = 0xcbf29ce484222325ULL;
    size_t total = 0;
    for (size_t off = 0; off < fsz;) {
        uint32_t n; memcpy(&n, buf + off, 4); off += 4;
        lxb_html_document_t *doc = lxb_html_document_create();
        lxb_html_document_parse(doc, buf + off, n); off += n;
        lxb_css_memory_t *mem = lxb_css_memory_create(); lxb_css_memory_init(mem, 128);
        lxb_css_parser_t *parser = lxb_css_parser_create(); lxb_css_parser_init(parser, NULL);
        lxb_css_parser_memory_set(parser, mem);
        lxb_css_selectors_t *css_sel = lxb_css_selectors_create(); lxb_css_selectors_init(css_sel);
        lxb_css_parser_selectors_set(parser, css_sel);
        lxb_selectors_t *s = lxb_selectors_create(); lxb_selectors_init(s);
        for (size_t k = 0; k < sizeof SELS / sizeof *SELS; k++) {
            lxb_css_selector_list_t *list = lxb_css_selectors_parse(parser, (const lxb_char_t *) SELS[k], strlen(SELS[k]));
            if (!list) { fprintf(stderr, "parse fail %s\n", SELS[k]); return 1; }
            for (int opt = 0; opt < 4; opt++) {
                for (int st = 0; st < 2; st++) {
                    lxb_selectors_opt_set(s, (opt & 1 ? LXB_SELECTORS_OPT_MATCH_FIRST : 0) | (opt & 2 ? LXB_SELECTORS_OPT_MATCH_ROOT : 0));
                    lxb_dom_node_t *roots[2] = { lxb_dom_interface_node(doc), doc->body ? lxb_dom_interface_node(doc->body) : NULL };
                    for (int r = 0; r < 2; r++) {
                        if (!roots[r]) continue;
                        ROOT = lxb_dom_interface_node(doc);
                        size_t cnt = 0; stop_after = st ? 3 : 0;
                        lxb_status_t status = lxb_selectors_find(s, roots[r], list, cb, &cnt);
                        mix(status); mix(cnt); total += cnt;
                    }
                }
            }
        }
        lxb_selectors_destroy(s, true); lxb_css_selectors_destroy(css_sel, true);
        lxb_css_parser_destroy(parser, true); lxb_css_memory_destroy(mem, true);
        lxb_html_document_destroy(doc);
    }
    printf("callbacks=%zu hash=%016llx\n", total, (unsigned long long) H);
    return 0;
}
