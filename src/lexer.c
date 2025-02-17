#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <regex.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "citron.h"

// TODO: implement state saves / restores for lexer_plug.c
// TOMOROW :^)
struct Lexer {

};
int ctr_clex_bflmt = 255;
ctr_size ctr_clex_tokvlen = 0; /* length of the string value of a token */
char *ctr_clex_buffer;
char *ctr_code;
char *ctr_code_st;
char *ctr_code_eoi;
char *ctr_clex_oldptr;
char *ctr_clex_olderptr;
ctr_bool ctr_clex_verbatim_mode = 0; /* flag: indicates whether lexer operates in
                                   verbatim mode or not (1 = ON, 0 = OFF) */
uintptr_t ctr_clex_verbatim_mode_insert_quote =
    0; /* pointer to 'overlay' the 'fake quote' for verbatim mode */
int ctr_clex_old_line_number = 0;
int ctr_transform_lambda_shorthand =
    0; /* flag: indicates whether lexer has seen a shorthand lambda (\(:arg)+
          expr) */

char *ctr_clex_desc_tok_ref = "reference";
char *ctr_clex_desc_tok_quote = "'";
char *ctr_clex_desc_tok_number = "number";
char *ctr_clex_desc_tok_paropen = "(";
char *ctr_clex_desc_tok_parclose = ")";
char *ctr_clex_desc_tok_blockopen = "{";
char *ctr_clex_desc_tok_blockopen_map = "{\\";
char *ctr_clex_desc_tok_blockclose = "}";
char *ctr_clex_desc_tok_tupopen = "[";
char *ctr_clex_desc_tok_tupclose = "]";
char *ctr_clex_desc_tok_colon = ":";
char *ctr_clex_desc_tok_dot = ".";
char *ctr_clex_desc_tok_chain = ",";
char *ctr_clex_desc_tok_booleanyes = "True";
char *ctr_clex_desc_tok_booleanno = "False";
char *ctr_clex_desc_tok_nil = "Nil";
char *ctr_clex_desc_tok_assignment = ":=";  // derp
char *ctr_clex_desc_tok_passignment = "=>"; // REEEE
char *ctr_clex_desc_tok_symbol = "\\"; // TODO FIXME Find a better character for this
char *ctr_clex_desc_tok_ret = "^";
char *ctr_clex_desc_tok_lit_esc = "$";
char *ctr_clex_desc_tok_ret_unicode = "↑";
char *ctr_clex_desc_tok_fin = "end of program";
char *ctr_clex_desc_tok_inv = "`";
char *ctr_clex_desc_tok_fancy_quot_open = "‹";
char *ctr_clex_desc_tok_fancy_quot_clos = "›";
char *ctr_clex_desc_tok_unknown = "(unknown token)";

int ctr_string_interpolation = 0;
char *ivarname;
int ivarlen;

#define MAX_LEXER_SAVE_STATES 1024
static struct lexer_state saved_lexer_states[MAX_LEXER_SAVE_STATES];
static int saved_lexer_state_next_index = 0;

#define MAX_LEXER_INJECT_VECTOR_COUNT 1024
static int inject_index = -1; //-1 -> nothing to inject
typedef struct {
  int token;
  int vlen;
  int real_vlen;
  char *value;
} token_inj_t;
static token_inj_t injects[MAX_LEXER_INJECT_VECTOR_COUNT]; // tokens to inject

/**
 * A helper that checks if a string starts with
 * another string.
 * returns 0 if it didn't match, and the length of the value
 * to match against if it did.
 * Should be a replacement for strncmp
 */
static size_t ctr_str_startswith(char *string, char *with) {
  size_t i;
  for (i = 0; with[i] != '\0'; ++i)
    if (string[i] != with[i])
      return 0;
  return i;
}

/**
 * Lexer - Inject token
 *
 * Injects a token to be returned before the next actual one
 * returns 0 unless there's not enough room in the lexer injection
 * vector
 */
int ctr_clex_inject_token(enum TokenType token, const char *value, const int vlen,
                          const int real_vlen) {
  if (inject_index == MAX_LEXER_INJECT_VECTOR_COUNT)
    return 1;
  // printf("Injecting (id=%d) %d (%.*s)\n", inject_index, token, vlen, value);
  token_inj_t inject = {
      token,
      vlen,
      real_vlen,
      (char *)value,
  };
  injects[++inject_index] = inject;
  return 0;
}

__attribute__((always_inline)) static int do_inject_token() {
  if (inject_index > -1) {
    token_inj_t inj = injects[inject_index--];
    if (inj.value) {
      strncpy(ctr_clex_buffer, inj.value, inj.real_vlen);
      ctr_clex_tokvlen = inj.vlen;
    }
    return inj.token;
  } else
    return -1;
}

/**
 * Lexer - Scan for character
 *
 * @return position of the encountered token or NULL if it doesn't exist
 */
char *ctr_clex_scan(char c) {
  char *start = ctr_code;
  while (*ctr_code != c) {
    if (*ctr_code == '\0') {
      // restore position
      ctr_code = start;
      return NULL;
    } else if (*ctr_code == '\n') {
      ++ctr_clex_line_number;
    }
    ++ctr_code;
  }
  return ctr_code;
}

/**
 * Lexer - Scan for balanced character
 *
 * @return position of the encountered token or NULL if it doesn't exist
 */
char *ctr_clex_scan_balanced(char c, char d) {
  char *backup = ctr_code;
  int bc = *ctr_code == d ? 1 : 0;
  if (*ctr_code == c)
    return ctr_code;
resume:
  while (*++ctr_code != c) {
    if (*ctr_code == '\0') {
      ctr_code = backup;
      return NULL;
    } else if (*ctr_code == '\n') {
      ctr_clex_line_number++;
    } else if (*ctr_code == d) {
      bc++;
    }
  }
  if (bc > 0 && --bc)
    goto resume;
  return ctr_code;
}

char *ctr_clex_read_balanced(char c, char d, int *line) {
  char *backup = ctr_code;
  int line_start = ctr_clex_line_number;
  char *res = ctr_clex_scan_balanced(c, d);
  ctr_code = backup;
  *line += ctr_clex_line_number - line_start;
  ctr_clex_line_number = line_start;
  return res;
}

static struct ctr_extension_descriptor {
  int ext;
  const char *name;
} ctr_clex_extensions[] = {{CTR_EXT_PURE_FS, "XPureLambda"},
                           {CTR_EXT_FROZEN_K, "XFrozen"},
#if withInlineAsm
                           {CTR_EXT_ASM_BLOCK, "XNakedAsmBlock"},
#endif
                           {0, NULL}};

size_t ctr_internal_edit_distance(const char *str1, const char *str2,
                                        size_t length1,
                                        size_t length2) {
  size_t *cache = ctr_heap_allocate(length1 * sizeof(unsigned int));
  size_t idx1;
  size_t idx2;
  size_t distance1;
  size_t distance2;
  size_t result;
  char code;

  /* Shortcut optimizations / degenerate cases. */
  if (str1 == str2)
    return 0;
  if (length1 == 0)
    return length2;
  if (length2 == 0)
    return length1;

  for (idx1 = 0; idx1 < length1; ++idx1)
    cache[idx1] = idx1 + 1;

  for (idx2 = 0; idx2 < length2; ++idx2) {
    code = str2[idx2];
    result = distance1 = idx2;

    for (idx1 = 0; idx1 < length1; ++idx1) {
      distance2 = code == str1[idx1] ? distance1 : distance1 + 1;
      distance1 = cache[idx1];

      cache[idx1] = result = distance1 > result
                              ?
                                distance2 > result
                                ?
                                  result + 1
                                :
                                  distance2
                              :
                                distance2 > distance1
                                ?
                                  distance1 + 1
                                :
                                  distance2;
    }
  }

  ctr_heap_free(cache);

  return result;
}

const char *ctr_clex_find_closest_extension(int length, char *val) {
  struct ctr_extension_descriptor *closest = NULL;
  unsigned int closested = length;
  for (struct ctr_extension_descriptor *current = ctr_clex_extensions;
       current->name; current++) {
    char const *name = current->name;
    if (ctr_internal_edit_distance(val, name, length, strlen(name)) < closested)
      closest = current;
  }
  return closest ? closest->name : "(¯\\_(ツ)_/¯)";
}

/**
 * Lexer - Save Lexer state
 *
 * saves the state of the lexer and
 * gives a unique ID used for restoring that state.
 */
int ctr_clex_save_state() {
  if (saved_lexer_state_next_index == MAX_LEXER_SAVE_STATES)
    return -1; // no more space left
  struct lexer_state ls = {ctr_string_interpolation,
                           ivarlen,
                           ctr_clex_tokvlen,
                           ctr_clex_buffer,
                           ctr_code,
                           ctr_code_st,
                           ctr_code_eoi,
                           ctr_clex_oldptr,
                           ctr_clex_olderptr,
                           ctr_clex_verbatim_mode,
                           ctr_clex_verbatim_mode_insert_quote,
                           ctr_clex_old_line_number,
                           ctr_transform_lambda_shorthand,
                           ivarname,
                           inject_index};
  saved_lexer_states[saved_lexer_state_next_index++] = ls;
  return saved_lexer_state_next_index - 1;
}

int ctr_clex_dump_state(struct lexer_state *ls) {
  ls->string_interpolation = ctr_string_interpolation;
  ls->ivarlen = ivarlen;
  ls->ctr_clex_tokvlen = ctr_clex_tokvlen;
  ls->ctr_clex_buffer = ctr_clex_buffer;
  ls->ctr_code = ctr_code;
  ls->ctr_code_st = ctr_code_st;
  ls->ctr_code_eoi = ctr_code_eoi;
  ls->ctr_clex_oldptr = ctr_clex_oldptr;
  ls->ctr_clex_olderptr = ctr_clex_olderptr;
  ls->ctr_clex_verbatim_mode = ctr_clex_verbatim_mode;
  ls->ctr_clex_verbatim_mode_insert_quote = ctr_clex_verbatim_mode_insert_quote;
  ls->ctr_clex_old_line_number = ctr_clex_old_line_number;
  ls->ctr_transform_lambda_shorthand = ctr_transform_lambda_shorthand;
  ls->ivarname = ivarname;
  ls->inject_index = inject_index;
  return 0;
}

/**
 * Lexer - Restore lexer state
 *
 * @param int id - id of the lexer state to restore
 * if id == TOS-1, set it free
 * @returns int - whether the last state was freed
 */
int ctr_clex_restore_state(int id) {
  assert(id < MAX_LEXER_SAVE_STATES);
  if (id == saved_lexer_state_next_index - 1)
    saved_lexer_state_next_index--;
  struct lexer_state ls = saved_lexer_states[id];
  ctr_string_interpolation = ls.string_interpolation;
  ivarlen = ls.ivarlen;
  ctr_clex_tokvlen = ls.ctr_clex_tokvlen;
  ctr_clex_buffer = ls.ctr_clex_buffer;
  ctr_code = ls.ctr_code;
  ctr_code_st = ls.ctr_code_st;
  ctr_code_eoi = ls.ctr_code_eoi;
  ctr_clex_oldptr = ls.ctr_clex_oldptr;
  ctr_clex_olderptr = ls.ctr_clex_olderptr;
  ctr_clex_verbatim_mode = ls.ctr_clex_verbatim_mode;
  ctr_clex_verbatim_mode_insert_quote = ls.ctr_clex_verbatim_mode_insert_quote;
  ctr_clex_old_line_number = ls.ctr_clex_old_line_number;
  ctr_transform_lambda_shorthand = ls.ctr_transform_lambda_shorthand;
  ivarname = ls.ivarname;
  inject_index = ls.inject_index;
  return id == saved_lexer_state_next_index;
}

int ctr_clex_load_state(struct lexer_state ls) {
  ctr_string_interpolation = ls.string_interpolation;
  ivarlen = ls.ivarlen;
  ctr_clex_tokvlen = ls.ctr_clex_tokvlen;
  ctr_clex_buffer = ls.ctr_clex_buffer;
  ctr_code = ls.ctr_code;
  ctr_code_st = ls.ctr_code_st;
  ctr_code_eoi = ls.ctr_code_eoi;
  ctr_clex_oldptr = ls.ctr_clex_oldptr;
  ctr_clex_olderptr = ls.ctr_clex_olderptr;
  ctr_clex_verbatim_mode = ls.ctr_clex_verbatim_mode;
  ctr_clex_verbatim_mode_insert_quote = ls.ctr_clex_verbatim_mode_insert_quote;
  ctr_clex_old_line_number = ls.ctr_clex_old_line_number;
  ctr_transform_lambda_shorthand = ls.ctr_transform_lambda_shorthand;
  ivarname = ls.ivarname;
  inject_index = ls.inject_index;
  return 0;
}

void ctr_clex_pop_saved_state() { saved_lexer_state_next_index--; }

/**
 * Lexer - is Symbol Delimiter ?
 * Determines whether the specified symbol is a delimiter.
 * Returns 1 if the symbol is a delimiter and 0 otherwise.
 *
 * @param char symbol symbol to be inspected
 *
 * @return uint8_t
 */
uint8_t ctr_clex_is_delimiter(char symbol) {

  return (symbol == '(' || symbol == '[' || symbol == ']' || symbol == ')' ||
          symbol == ',' || symbol == '.' || symbol == ':' || symbol == ' ' ||
          symbol == '\t' || symbol == '{' || symbol == '}' || symbol == '#');
}

unsigned long ctr_clex_position() {
  return ctr_code - ctr_code_st - ctr_clex_tokvlen;
}

/**
 * CTRLexerEmitError
 *
 * Displays an error message for the lexer.
 */
void ctr_clex_emit_error(char *message) {
#ifdef EXIT_ON_ERROR
  fprintf(stderr, "%s on line: %d. \n", message, ctr_clex_line_number);
  exit(1);
#else
  CtrStdFlow =
      ctr_format_str("E%s on line: %d.\n", message, ctr_clex_line_number);
#endif
}

/**
 * CTRLexerLoad
 *
 * Loads program into memory.
 */
void ctr_clex_load(char *prg) {
  ctr_code = prg;
  ctr_code_st = ctr_code;
  ctr_clex_buffer = ctr_heap_allocate_tracked(ctr_clex_bflmt);
  ctr_clex_buffer[0] = '\0';
  ctr_clex_line_number = 0;
  ctr_clex_code_init = ctr_code;
}

/**
 * CTRLexerTokenValue
 *
 * Returns the string of characters representing the value
 * of the currently selected token.
 */
char *ctr_clex_tok_value() { return ctr_clex_buffer; }

char *ctr_clex_tok_describe(enum TokenType token) {
  switch (token) {
  case TokenTypeRet:           return ctr_clex_desc_tok_ret;
  case TokenTypeAssignment:    return ctr_clex_desc_tok_assignment;
  case TokenTypePassignment:   return ctr_clex_desc_tok_passignment;
  case TokenTypeBlockclose:    return ctr_clex_desc_tok_blockclose;
  case TokenTypeBlockopen:     return ctr_clex_desc_tok_blockopen;
  case TokenTypeBlockopenMap:  return ctr_clex_desc_tok_blockopen_map;
  case TokenTypeBooleanno:     return ctr_clex_desc_tok_booleanno;
  case TokenTypeBooleanyes:    return ctr_clex_desc_tok_booleanyes;
  case TokenTypeChain:         return ctr_clex_desc_tok_chain;
  case TokenTypeColon:         return ctr_clex_desc_tok_colon;
  case TokenTypeDot:           return ctr_clex_desc_tok_dot;
  case TokenTypeFin:           return ctr_clex_desc_tok_fin;
  case TokenTypeNil:           return ctr_clex_desc_tok_nil;
  case TokenTypeNumber:        return ctr_clex_desc_tok_number;
  case TokenTypeParclose:      return ctr_clex_desc_tok_parclose;
  case TokenTypeParopen:       return ctr_clex_desc_tok_paropen;
  case TokenTypeQuote:         return ctr_clex_desc_tok_quote;
  case TokenTypeRef:           return ctr_clex_desc_tok_ref;
  case TokenTypeTupopen:       return ctr_clex_desc_tok_tupopen;
  case TokenTypeTupclose:      return ctr_clex_desc_tok_tupclose;
  case TokenTypeSymbol:        return ctr_clex_desc_tok_symbol;
  case TokenTypeLiteralEsc:    return ctr_clex_desc_tok_lit_esc;
  case TokenTypeInv:           return ctr_clex_desc_tok_inv;
  case TokenTypeFancyQuotOpen: return ctr_clex_desc_tok_fancy_quot_open;
  case TokenTypeFancyQuotClos: return ctr_clex_desc_tok_fancy_quot_clos;
  default:                     return ctr_clex_desc_tok_unknown;
  }
}

/**
 * CTRLexerTokenValueLength
 *
 * Returns the length of the value of the currently selected token.
 */
long ctr_clex_tok_value_length() { return ctr_clex_tokvlen; }

/**
 * CTRLexerPutBackToken
 *
 * Puts back a token and resets the pointer to the previous one.
 */
void ctr_clex_putback() {
  if (ctr_string_interpolation > 0) {
    ctr_string_interpolation--;
    return;
  }
  ctr_code = ctr_clex_oldptr;
  ctr_clex_oldptr = ctr_clex_olderptr;
  ctr_clex_line_number = ctr_clex_old_line_number;
}

int ctr_clex_is_valid_digit_in_base(char c, int b) {
  if (b <= 10) {
    if ((c >= '0') && (c < ('0' + b)))
      return 1;
  } else if (b <= 36) {
    if ((c >= '0') && (c <= '9'))
      return 1;
    else if ((c >= 'A') && (c < 'A' + (b - 10)))
      return 1;
  }
  return 0; // unsupported base or not a digit
}

ctr_bool check_next_line_empty() {
  if (!regexLineCheck)
    return ctr_code[1] != '\n';

  switch (regexLineCheck->value) {
  case 0:
    return ctr_code[1] != '\n';
  case 1: {
    regex_t pattern;
    ctr_bool res;
    if (regcomp(&pattern, "^$", 0) != 0)
      ctr_clex_emit_error("PCRE could not compile regex, please turn regexLineCheck off.");

    res = regexec(&pattern, ctr_code + 1, 0, NULL, 0) == REG_NOMATCH;
    regfree(&pattern);
    return res;
  }
  }
  return false;
}

static bool ctr_clex_identifier_name(char *name) {
  size_t len;
  if (len = ctr_str_startswith(ctr_code, name)) {
    if (ctr_clex_is_delimiter(ctr_code[len])) {
      ctr_code += len;
      return true;
    }
  }
  return false;
}

static ctr_bool ctr_clex_number() {
  size_t i = 0;
  char c = ctr_code[0];
  int base = 10;
  if ((c != '-' || !isdigit(ctr_code[1])) && !isdigit(c))
    return false;
  ctr_clex_buffer[i++] = c;
  ++ctr_clex_tokvlen;
  c = toupper(*++ctr_code);
  if (ctr_code[-1] == '0') {
    switch (c) {
    case 'X': base = 16; break;
    case 'C': base = 8; break;
    case 'B': base = 2; break;
    }
  }
  if (base != 10) {
    if (c != 'C') {
      ctr_clex_buffer[i++] = c;
      ctr_clex_tokvlen++;
    }
    c = toupper(*++ctr_code);
  }
  while ((ctr_clex_is_valid_digit_in_base(c, base))) {
    ctr_clex_buffer[i++] = c;
    ctr_clex_tokvlen++;
    c = toupper(*++ctr_code);
  }
  if (c == '.') {
    if (!ctr_clex_is_valid_digit_in_base(toupper(ctr_code[1]), base))
      return true;
    do {
      ctr_clex_buffer[i++] = c;
      ctr_clex_tokvlen++;
      c = toupper(*++ctr_code);
    } while((ctr_clex_is_valid_digit_in_base(c, base)));
  }
  return true;
}

/**
 * CTRActivatePragma
 *
 *	Activates a pragma based off its type
 *	t -> toggle, o -> one-shot (Cannot be deactivated)
 */
void ctr_activate_pragma(ctr_code_pragma *pragma) {
  switch (pragma->type) {
  case 'o':
    pragma->value = 1;
    break;
  case 't':
    pragma->value = 1 - pragma->value;
    break;
  }
}

void ctr_set_pragma(ctr_code_pragma *pragma, int val, int val2) {
  pragma->value = val;
  pragma->value_e = val2;
}

extern ctr_tnode *ctr_cparse_block_(int);
/**
 * CTRLexPragmaToken
 *
 * Reads the token after '#:' and toggles a pragma.
 *
 */
__attribute__((always_inline)) static void handle_extension() {
  char *ext = ctr_clex_buffer;
  size_t len;
#ifdef DEBUG_BUILD
  fprintf(stderr, "+ext %.*s\n", len, ext);
#endif
  if (len = ctr_str_startswith(ext, "XFrozen")) {
    extensionsPra->value |= CTR_EXT_FROZEN_K;
  } else if (len = ctr_str_startswith(ext, "XPureLambda")) {
    extensionsPra->value |= CTR_EXT_PURE_FS;
  }
  #if withInlineAsm
  else if (len = ctr_str_startswith(ext, "XNakedAsmBlock")) {
    extensionsPra->value |= CTR_EXT_ASM_BLOCK;
  }
  #endif
  else {
    static char errbuf[1024];
    sprintf(errbuf, "Unknown extension '%.*s' did you mean '%s'?", len, ext,
            ctr_clex_find_closest_extension(len, ext));
    ctr_clex_emit_error(errbuf);
  }
}

void ctr_match_toggle_pragma() {
  size_t l;
  if (l = ctr_str_startswith(ctr_code, ":oneLineExpressions")) {
    ctr_activate_pragma(oneLineExpressions);
    ctr_code += l;
    return;
  } else if (l = ctr_str_startswith(ctr_code, ":regexLineCheck")) {
    ctr_activate_pragma(regexLineCheck);
    ctr_code += l;
    return;
  } else if (l = ctr_str_startswith(ctr_code, ":flexibleConstructs")) {
    ctr_activate_pragma(flexibleConstructs);
    ctr_code += l;
    return;
  } else if (l = ctr_str_startswith(ctr_code, ":autofillHoles")) {
    ctr_activate_pragma(autofillHoles);
    ctr_code += l;
    return;
  } else if (l = ctr_str_startswith(ctr_code, ":callShorthand")) {
    ctr_code += l;
    enum TokenType t0 = ctr_clex_tok(), t1 = ctr_clex_tok();
    ctr_set_pragma(callShorthand, t0, t1);
    // while(isspace(*ctr_clex_oldptr)) ctr_clex_oldptr++; //no chance of it
    // falling off ctr_clex_oldptr++; while(*(ctr_code--) != '\n'); //go back
    // out
    ctr_clex_olderptr = ctr_code;
    ctr_clex_oldptr = ctr_code;
    ctr_code--;
  } else if (l = ctr_str_startswith(ctr_code, ":declare")) {
    ctr_code += l;
    int t0 = ctr_clex_tok();
    if (t0 != TokenTypeRef)
      goto err;
    char *v = ctr_clex_tok_value();
    int len = ctr_clex_tok_value_length();
    int fixity = 0;
    int prec = 2;
    int lazy = 0;
    if (len != strlen("infixr"))
      goto err;
    if (ctr_str_startswith(v, "infixr"))
      fixity = 0;
    else if (ctr_str_startswith(v, "infixl"))
      fixity = 1;
    else if (ctr_str_startswith(v, "lazyev"))
      prec = lazy = 1;
    else
      goto err;
    t0 = ctr_clex_tok();
    if (t0 == TokenTypeNumber) {
      prec = atoi(ctr_clex_tok_value());
      t0 = ctr_clex_tok();
    }
    if (t0 != TokenTypeRef) {
      if (t0 == TokenTypeColon && lazy) {
        // next call is lazy
        nextCallLazy->value = prec;
        goto ending;
      }
      ctr_clex_emit_error("Expected some op name");
      return;
    }
    v = ctr_clex_tok_value();
    len = ctr_clex_tok_value_length();
    ctr_set_fix(v, len, fixity, prec, lazy);
  ending:
    ctr_clex_olderptr = ctr_code;
    ctr_clex_oldptr = ctr_code;
    return;
  err:
    ctr_clex_emit_error("Expected either infixr or infixl or lazyev");
    return;
  } else if (l = ctr_str_startswith(ctr_code, ":language")) {
    // #:language ext,ext,ext
    int lineno = ctr_clex_line_number;
    ctr_code += l;
    int t0 = ctr_clex_tok();
    if (t0 != TokenTypeRef || lineno != ctr_clex_line_number) {
    err_v:
      ctr_clex_emit_error("Expected an extension name");
      return;
    }
    handle_extension();
    // printf("+ %.*s\n", ctr_clex_tokvlen, ctr_clex_buffer);
    while (ctr_clex_tok() == TokenTypeChain) {
      if (lineno != ctr_clex_line_number)
        break;
      t0 = ctr_clex_tok();
      if (t0 != TokenTypeRef)
        goto err_v;
      // printf("+ %.*s\n", ctr_clex_tokvlen, ctr_clex_buffer);
      handle_extension();
    }
    ctr_clex_putback();
    ctr_clex_olderptr = ctr_code;
    ctr_clex_oldptr = ctr_code;
    return;
  }
}

/**
 * CTRLexerReadToken
 *
 * Reads the next token from the program buffer and selects this
 * token.
 */
enum TokenType ctr_clex_tok() {
  int tinj;
  if ((tinj = do_inject_token()) != -1) {
    ctr_clex_olderptr = ctr_clex_oldptr;
    ctr_clex_oldptr = ctr_code;
    return tinj;
  }
  if (ctr_code[0] == '\0') {
    return TokenTypeFin;
  }
  char c;
  int i, comment_mode, presetToken, pragma_mode;
  ctr_clex_tokvlen = 0;
  ctr_clex_olderptr = ctr_clex_oldptr;
  ctr_clex_oldptr = ctr_code;
  ctr_clex_old_line_number = ctr_clex_line_number;
  i = 0;
  comment_mode = 0;
  pragma_mode = 0;

  /* a little state machine to handle string interpolation, */
  /* i.e. transforms ' $$x ' into: ' ' + x + ' '. */
  /* 'x'$${y}z' -> 'x' + ( y ) + 'z' */
  switch (ctr_string_interpolation) {
  // $$ref
  case 1:
    presetToken = TokenTypeQuote;
    break;
  case 2:
  case 4:
    memcpy(ctr_clex_buffer, "+", 1);
    ctr_clex_tokvlen = 1;
    presetToken = TokenTypeRef;
    break;
  case 3:
    memcpy(ctr_clex_buffer, ivarname, ivarlen);
    ctr_clex_tokvlen = ivarlen;
    presetToken = TokenTypeRef;
    break;
  case 5:
    ctr_code = ctr_code_eoi;
    presetToken = TokenTypeQuote;
    break;
  }
  /* return the preset token, and transition to next state */
  if (ctr_string_interpolation) {
    ctr_string_interpolation++;
    return presetToken;
  }

  /* if verbatim mode is on and we passed the '>' verbatim write message, insert
   * a 'fake quote' (?>') */
  if (ctr_clex_verbatim_mode == 1 &&
      ctr_clex_verbatim_mode_insert_quote == (uintptr_t)ctr_code) {
    return TokenTypeQuote;
  }

  if (ctr_code[0] != '\0' && *ctr_code == '\n' && check_next_line_empty() &&
      oneLineExpressions->value) {
    ctr_code++;
    return TokenTypeDot;
  }

  c = *ctr_code;
  while (*ctr_code != '\0' && (isspace(c) || c == '#' || comment_mode)) {
    if (c == '\n') {
      comment_mode = 0;
      pragma_mode = 0;
      ctr_clex_line_number++;
    }
    if (c == '#') {
      comment_mode = 1;
      if (ctr_code[1] == ':')
        pragma_mode = 1;
    }
    if (pragma_mode)
      ctr_match_toggle_pragma();
    ctr_code++;
    c = *ctr_code;
  }
  if (ctr_code[0] == '\0') {
    return TokenTypeFin;
  }
  if (c == '\\') {
    ctr_code++;
    int t = ctr_clex_tok();
    ctr_clex_putback();
    if (t != TokenTypeRef) {
      if (t == TokenTypeColon) { // transform \:x expr to {\:x expr}
        ctr_transform_lambda_shorthand = 1;
        return TokenTypeBlockopenMap; // HACK This thing here simply
                                        // transforms the syntax, however we
                                        // fool the in-language lexer to think
                                        // that this is actually a ref
      } else { // if not a (\:x expr) then it's simply a message
        ctr_clex_buffer[0] = '\\';
        ctr_clex_tokvlen = 1;
        return TokenTypeRef;
      }
      // ctr_clex_emit_error("Expected a reference");
    }
    return TokenTypeSymbol;
  }
  if (c == '$' && ctr_code[1] != '\0') {
    char _t = *(++ctr_code);
    int q = 0;
    /* The lexer state should this succeed
     *   $(expr)
     *   ^
     */
    switch (_t) {
    case '(':
      ctr_clex_tokvlen = -1; // literal escape mode
      return TokenTypeLiteralEsc;
    case '[':
      ctr_clex_tokvlen = -2; // tuple escape mode
      return TokenTypeLiteralEsc;
    case '`': // literal replace mode
      q = 2;
    case '\'': // quote
      if (!q)
        q = 1;
    case '!': // literal unescape
      if (ctr_code[1] != '\0' && !isspace(*(++ctr_code))) {
        ctr_clex_tokvlen = -3 - q; // unescape mode (q=1 quote)
        return TokenTypeLiteralEsc;
      }
      ctr_code--;
    }
    ctr_code--;
    /* Fallthrough */
  }
  if (c == '(') {
    ctr_code++;
    return TokenTypeParopen;
  }
  if (c == ')') {
    ctr_code++;
    return TokenTypeParclose;
  }
  if (c == '[') {
    ctr_code++;
    return TokenTypeTupopen;
  }
  if (c == ']') {
    ctr_code++;
    return TokenTypeTupclose;
  }
  if (c == '{') {
    ctr_code++;
    if ((c = *ctr_code) == '\\') {
      ctr_code++;
      return TokenTypeBlockopenMap;
    } else
      return TokenTypeBlockopen;
  }
  if (c == '}') {
    ctr_code++;
    return TokenTypeBlockclose;
  }
  if (c == '.') {
    ctr_code++;
    return TokenTypeDot;
  }
  if (c == ',') {
    ctr_code++;
    return TokenTypeChain;
  }
  if ((c == 'i' && ctr_code[1] == 's' && isspace(ctr_code[2])) ||
      ((c == ':') && (ctr_code[1] == '='))) {
    ctr_code += 2;
    return TokenTypeAssignment;
  }
  if (c == '=' && ctr_code[1] == '>') {
    ctr_code += 2;
    return TokenTypePassignment;
  }
  if (c == ':' /*&& ctr_code[1] != ':' */) {
    int is_name = 0;
    while (ctr_code[1] == ':') {
      is_name++;
      ctr_code++;
      ctr_clex_buffer[i++] = ':';
      ctr_clex_tokvlen++;
    }
    ctr_code++;
    if (is_name) {
      if (ctr_clex_tokvlen > 1)
        ctr_code--;
      return TokenTypeRef;
    }
    return TokenTypeColon;
  }
  if (c == '^') {
    ctr_code++;
    return TokenTypeRet;
  }
  //↑
  if ((uint8_t)c == 226 &&
      ((uint8_t) * (ctr_code + 1) == 134) &&
      ((uint8_t) * (ctr_code + 2) == 145)) {
    ctr_code += 3;
    return TokenTypeRet;
  }
  if (c == -30) {
    // ‹›
    if (ctr_code[1] == -128) {
      if (ctr_code[2] == -71)
        return TokenTypeFancyQuotOpen;
      if (ctr_code[2] == -72)
        return TokenTypeFancyQuotClos;
    }
  }
  if (c == '\'') {
    ctr_code++;
    return TokenTypeQuote;
  }
  if (ctr_clex_number())
    return TokenTypeNumber;
  if (c == '`') {
    struct lexer_state st;
    ctr_code++;
    ctr_clex_dump_state(&st);
    int t = ctr_clex_tok(), rv = 0;
    if (t == TokenTypeRef) {
      if (ctr_clex_buffer[ctr_clex_tokvlen - 1] == '`')
        rv = 1;
    }
    ctr_clex_load_state(st);
    if (rv)
      return TokenTypeInv;
    ctr_code--;
  }

  if (ctr_clex_identifier_name("True"))
    return TokenTypeBooleanyes;
  if (ctr_clex_identifier_name("False"))
    return TokenTypeBooleanno;
  if (ctr_clex_identifier_name("Nil"))
    return TokenTypeNil;

  /* if we encounter a '?>' sequence, switch to verbatim mode in lexer */
  if (ctr_str_startswith(ctr_code, "?>")) {
    ctr_clex_verbatim_mode = 1;
    ctr_code += 2;
    return TokenTypeQuote;
  }

  /* if lexer is in verbatim mode and we pass the '>' symbol insert a fake quote
   * as next token */
  if (ctr_code[0] == '>' && ctr_clex_verbatim_mode) {
    // ctr_clex_verbatim_mode_insert_quote = (uintptr_t) (ctr_code + 1);      /*
    // this way because multiple invocations should return same result */
    return TokenTypeQuote;
  }
  while (!isspace(c) &&
         (c != '#' && c != '(' && c != ')' && c != '[' && c != ']' &&
          c != '{' && c != '}' && c != '.' && c != ',' && c != '^' &&
          (!((uint8_t)c == 226 &&
             ((uint8_t) * (ctr_code + 1) == 134) &&
             ((uint8_t) * (ctr_code + 2) == 145))) &&
          (c != ':') && c != '\'') &&
         ctr_code[0] != '\0') {
    ctr_clex_buffer[i] = c;
    ctr_clex_tokvlen++;
    i++;
    if (i > ctr_clex_bflmt) {
      ctr_clex_emit_error(
          "Token Buffer Exausted. Tokens may not exceed 255 bytes");
    }
    ctr_code++;
    if (ctr_code[0] == '\0')
      return TokenTypeRef;
    c = ctr_code[0];
  }
  return TokenTypeRef;
}

static int ctr_clex_next_number() {
  int t = *(++ctr_code);
  return t >= '0' && t <= '9' ? t - '0' : 10 + toupper(t) - 'A';
}

/**
 * CtrLexerEncodeUTF-8
 *
 * Encodes a code point using UTF-8
 */
int ctr_clex_utf8_encode(char *out, uint32_t utf) {
  if (utf <= 0x7F) {
    // Plain ASCII
    out[0] = (char)utf;
    out[1] = 0;
    return 1;
  } else if (utf <= 0x07FF) {
    // 2-byte unicode
    out[0] = (char)(((utf >> 6) & 0x1F) | 0xC0);
    out[1] = (char)(((utf >> 0) & 0x3F) | 0x80);
    out[2] = 0;
    return 2;
  } else if (utf <= 0xFFFF) {
    // 3-byte unicode
    out[0] = (char)(((utf >> 12) & 0x0F) | 0xE0);
    out[1] = (char)(((utf >> 6) & 0x3F) | 0x80);
    out[2] = (char)(((utf >> 0) & 0x3F) | 0x80);
    out[3] = 0;
    return 3;
  } else if (utf <= 0x10FFFF) {
    // 4-byte unicode
    out[0] = (char)(((utf >> 18) & 0x07) | 0xF0);
    out[1] = (char)(((utf >> 12) & 0x3F) | 0x80);
    out[2] = (char)(((utf >> 6) & 0x3F) | 0x80);
    out[3] = (char)(((utf >> 0) & 0x3F) | 0x80);
    out[4] = 0;
    return 4;
  } else {
    // error - use replacement character
    out[0] = (char)0xEF;
    out[1] = (char)0xBF;
    out[2] = (char)0xBD;
    out[3] = 0;
    return 0;
  }
}

char *ctr_clex_readfstr() {
  char *strbuff;
  char c;
  long memblock = 1;
  long page = 64;
  char *beginbuff;
  size_t tracking_id;

  ctr_clex_tokvlen = 0;
  strbuff = ctr_heap_allocate_tracked(memblock);
  tracking_id = ctr_heap_get_latest_tracking_id();
  c = *(ctr_code += 3);
  beginbuff = strbuff;
  while (1) {
    if (c == 0)
      break;
    if (c == '\n')
      ctr_clex_line_number++;
    if (c == -30) {
      if (ctr_code[1] == -128 && ctr_code[2] == -70) {
        break;
      }
    }
    ctr_clex_tokvlen++;
    if (ctr_clex_tokvlen >= memblock) {
      memblock += page;
      beginbuff = (char *)ctr_heap_reallocate_tracked(tracking_id, memblock);
      if (beginbuff == NULL) {
        ctr_clex_emit_error("Out of memory");
      }
      /* reset pointer, memory location might have been changed */
      strbuff = beginbuff + (ctr_clex_tokvlen - 1);
    }
    *(strbuff++) = c;
    ctr_code++;
    if (ctr_code[0] != '\0')
      c = ctr_code[0];
    else {
      ctr_clex_emit_error("Expected closing quote");
      c = '\'';
    }
  }

  return beginbuff;
}

/**
 * CTRLexerStringReader
 *
 * Reads an entire string between a pair of quotes.
 */
char *ctr_clex_readstr() {
  char *strbuff;
  char c;
  long memblock = 1;
  ctr_bool escape = false;
  char *beginbuff;
  long page = 100; /* 100 byte pages */
  size_t tracking_id;

  if (ctr_string_interpolation == 6) {
    ctr_heap_free(ivarname);
    ctr_string_interpolation = 0;
  }

  ctr_clex_tokvlen = 0;
  strbuff = (char *)ctr_heap_allocate_tracked(memblock);
  tracking_id = ctr_heap_get_latest_tracking_id();
  c = ctr_code[0];
  beginbuff = strbuff;
  while (/* reading string in non-verbatim mode, read until the first non-escaped quote */
         (!ctr_clex_verbatim_mode && (c != '\'' || escape == 1)) ||
         /* reading string in verbatim mode, read until the '<?' sequence */
         (ctr_clex_verbatim_mode && ctr_code[0] != '\0' && (c != '<' || ctr_code[1] == '\0' || ctr_code[1] != '?'))) {

    /* enter interpolation mode ( $$x ) */
    if (!ctr_clex_verbatim_mode && !escape && c == '$') {
      /* expression interpolation ( ${{some expr}}$ ) */
      if (ctr_code[1] == '{' &&
          ctr_code[2] == '{') {
        // transform the source _in place_
        char *end = ctr_clex_read_balanced('}', '{', &ctr_clex_line_number);
        if (!end) {
          ctr_clex_emit_error("Invalid use of interpolation");
        } else {
          c = '\'';
          ctr_code[0] = '\'';
          ctr_code[1] = '+';
          ctr_code[2] = '(';
          end[-1] = ')';
          end[0] = '+';
          end[1] = '\'';
          break;
        }
      } else if (ctr_code[1] == '$') {
        int q = 2;
        while (!isspace(ctr_code[q]) &&
               ctr_code[q] != '$' && ctr_code[q] != '\'' && q < 255)
          q++;
        if (isspace(ctr_code[q]) || ctr_code[q] == '$' ||
            ctr_code[q] == '\'') {
          ivarname = ctr_heap_allocate(q);
          ivarlen = q - 2;
          memcpy(ivarname, ctr_code + 2, q - 2);
          ctr_string_interpolation = 1;
          ctr_code_eoi = ctr_code + q; /* '$','$' and the name  ( name + 3 ) */
          break;
        }
      }
    }

    if (c == '\n')
      ctr_clex_line_number++;

    if (escape == 1) {
      switch (c) {
      case 'n':
        c = '\n';
        break;
      case 'r':
        c = '\r';
        break;
      case 't':
        c = '\t';
        break;
      case 'v':
        c = '\v';
        break;
      case 'b':
        c = '\b';
        break;
      case 'a':
        c = '\a';
        break;
      case 'f':
        c = '\f';
        break;
      case 'x': {
        int is_explicitly_enclosed;
        ctr_code += (is_explicitly_enclosed = ctr_code[1] == '{');
        c = 0;
        while ((is_explicitly_enclosed)
                   ? ctr_code[1] != '}' && ctr_clex_is_valid_digit_in_base(
                                                   toupper(ctr_code[1]), 16)
                   : ctr_clex_is_valid_digit_in_base(toupper(ctr_code[1]),
                                                     16)) {
          c = c * 16 + ctr_clex_next_number();
        }
        if (is_explicitly_enclosed && *(++ctr_code) != '}') {
          // bitch about it...
          static char err[] =
              "Expected a '}' to close the explicit hex embed, not a '$'";
          err[55] = ctr_code[1];
          ctr_clex_emit_error(err);
        }
        break;
      }
      case '0': case '1': case '2': case '3':
      case '4': case '5': case '6': case '7': {
        // C style \xxx escape sequences
        uint16_t v = 0;
        size_t i;
        c = 0;
        for (i = 0; i < 3 && ctr_clex_is_valid_digit_in_base(ctr_code[i], 8); ++i) {
          v = v * 8 + (ctr_code[i] - '0');
          if (v > 127) break;
          c = c * 8 + (ctr_code[i] - '0');
          ctr_code;
        }
        ctr_code += i - 1;
        break;
      }
      case 'u': {
        const char hexs[] = "0123456789ABCDEF";
        uint32_t uc = 0, is_explicitly_enclosed;
        ctr_code += (is_explicitly_enclosed = ctr_code[1] == '{');
        while (1) {
          char cc = ctr_code[1];
          if (is_explicitly_enclosed && cc == '}') {
            break;
          }
          cc = toupper(cc);
          if (!ctr_clex_is_valid_digit_in_base(cc, 16))
            break;
          char *h = strchr(hexs, cc);
          uc = (uc << 4) + ((int)(h - hexs));
          ctr_code++;
        }
        if (is_explicitly_enclosed && ctr_code[1] != '}') {
          // bitch about it...
          static char err[] =
              "Expected a '}' to close the explicit unicode embed, not a '$'";
          err[59] = ctr_code[1];
          ctr_clex_emit_error(err);
        }
        if (uc > 0x100FFFF) {
          ctr_clex_emit_error("Character code out of range");
          if (is_explicitly_enclosed)
            ctr_code++;
          c = 0;
          break;
        }
        static char pc[5];
        int count = ctr_clex_utf8_encode(pc, uc);
        c = pc[0];
        for (int i = 0; i < count - 1; i++) {
          ctr_clex_tokvlen++;
          if (ctr_clex_tokvlen >= memblock) {
            memblock += page;
            beginbuff =
                (char *)ctr_heap_reallocate_tracked(tracking_id, memblock);
            if (beginbuff == NULL) {
              ctr_clex_emit_error("Out of memory");
            }
            /* reset pointer, memory location might have been changed */
            strbuff = beginbuff + (ctr_clex_tokvlen - 1);
          }
          *(strbuff++) = c;
          c = pc[i + 1];
        }
        if (is_explicitly_enclosed) {
          ctr_code++;
          continue;
        }
      }
      }
    }

    if (c == '\\' && escape == 0 && ctr_clex_verbatim_mode == 0) {
      escape = 1;
      ctr_code++;
      c = *ctr_code;
      continue;
    }
    ctr_clex_tokvlen++;
    if (ctr_clex_tokvlen >= memblock) {
      memblock += page;
      beginbuff = (char *)ctr_heap_reallocate_tracked(tracking_id, memblock);
      if (beginbuff == NULL) {
        ctr_clex_emit_error("Out of memory");
      }
      /* reset pointer, memory location might have been changed */
      strbuff = beginbuff + (ctr_clex_tokvlen - 1);
    }
    escape = 0;
    *(strbuff++) = c;
    ctr_code++;
    if (ctr_code[0] != '\0')
      c = *ctr_code;
    else {
      ctr_clex_emit_error("Expected closing quote");
      c = '\'';
    }
  }
  if (ctr_clex_verbatim_mode) {
    if (ctr_code[0] == '\0') { /* if we reached EOF in verbatim mode, append
                                      closing sequence '<?.' */
      strncpy(ctr_code, "<?.", 3);
    }
    ctr_code++; /* in verbatim mode, hop over the trailing ? as well */
  }
  ctr_clex_verbatim_mode = 0; /* always turn verbatim mode off */
  ctr_clex_verbatim_mode_insert_quote =
      0; /* erase verbatim mode pointer overlay for fake quote */
  return beginbuff;
}
