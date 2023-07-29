#include "9cc.h"

// ローカル変数
LVar *locals;

bool at_eof() {
  return token->kind == TK_EOF;
}

// 次のトークンが期待している記号のときには、トークンを1つ読み進める。
// それ以外の場合にはエラーを報告する。
void expect(char *op) {
  if (token->kind != TK_RESERVED ||
      strlen(op) != token->len ||
      memcmp(token->str, op, token->len)) {
    error_at(token->str, "\"%s\"ではありません", op);
  }
  token = token->next;
}

// 次のトークンが数値の場合、トークンを1つ読み進めてその数値を返す。
// それ以外の場合にはエラーを報告する。
int expect_number() {
  if (token->kind != TK_NUM) error_at(token->str, "数ではありません");
  int val = token->val;
  token = token->next;
  return val;
}

Node *new_node(NodeKind kind, Node *lhs, Node *rhs) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = kind;
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

Node *new_node_num(int val) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = ND_NUM;
  node->val = val;
  return node;
}

// 次のトークンが期待している記号のときには、トークンを1つ読み進めて
// 真を返す。それ以外の場合には偽を返す。
bool consume(char *op, TokenKind tk_kind) {
  if (token->kind != tk_kind ||
      strlen(op) != token->len ||
      memcmp(token->str, op, token->len)) {
    return false;
  }
  token = token->next;
  return true;
}

// トークンが識別子であればそのトークンを返し、トークンを1つ進める
Token *consume_ident() {
  if (token->kind != TK_IDENT) {
    return NULL;
  }
  Token *current_token = token;
  token = token->next;
  return current_token;
}

// 変数を名前で検索する。見つからなかった場合はNULLを返す。
LVar *find_lvar(Token *tok) {
  for (LVar *var = locals; var; var = var->next) {
    if (var->len == tok->len && !memcmp(tok->str, var->name, var->len)) {
      return var;
    }
  }
  return NULL;
}

Node *code[100];

// 生成規則: program = stmt*
void program() {
  int i = 0;
  while (!at_eof()) {
    // stmtは「;」の手前までを構文木にして返すので、codeの配列に各構文木を入れる
    code[i++] = stmt();
  }
  code[i] = NULL;
}

// 生成規則: stmt = expr ";"
//               | "{" stmt* "}"
//               | "return" expr ";"
//               | "if" "(" expr ")" stmt ("else" stmt)?
//               | "while" "(" expr ")" stmt
//               | "for" "(" expr? ";" expr? ";" expr? ")" stmt
Node *stmt() {
  Node *node;
  if (consume("return", TK_RETURN)) {
    node = calloc(1, sizeof(Node));
    node->kind = ND_RETURN;
    node->lhs = expr();
  } else if (consume("if", TK_IF)) {
    node = calloc(1, sizeof(Node));
    node->kind = ND_IF;
    expect("(");
    // ()内の条件式をlhsに代入
    node->lhs = expr();
    expect(")");
    // if内の処理をrhsに代入
    node->rhs = stmt();
    if (consume("else", TK_ELSE)) {
      Node *els = calloc(1, sizeof(Node));
      els->kind = ND_ELSE;
      els->lhs = node->rhs;
      els->rhs = stmt();
      node->rhs = els;
    }
    // stmtの後に「;」が来ることはないのでここでreturnする必要がある
    return node;
  } else if (consume("while", TK_WHILE)) {
    node = calloc(1, sizeof(Node));
    node->kind = ND_WHILE;
    expect("(");
    node->lhs = expr();
    expect(")");
    node->rhs = stmt();
    return node;
  } else if (consume("for", TK_FOR)) {
    node = calloc(1, sizeof(Node));
    node->kind = ND_FOR;
    Node *left = calloc(1, sizeof(Node));
    Node *right = calloc(1, sizeof(Node));
    expect("(");
    left->lhs = expr();
    expect(";");
    left->rhs = expr();
    expect(";");
    right->lhs = expr();
    expect(")");
    right->rhs = stmt();
    node->lhs = left;
    node->rhs = right;
    return node;
  } else if (consume("{", TK_RESERVED)) {
    node = calloc(1, sizeof(Node));
    node->kind = ND_BLOCK;
    node->block = calloc(100, sizeof(Node));
    for (int i = 0; !consume("}", TK_RESERVED); i++) {
      node->block[i] = stmt();
    }
    return node;
  } else {
    node = expr();
  }
  expect(";");
  return node;
}

// 生成規則: expr = assign
Node *expr() {
  return assign();
}

// 生成規則: assign = equality ("=" assign)?
Node *assign() {
  Node *node = equality();
  if (consume("=", TK_RESERVED)) {
    node = new_node(ND_ASSIGN, node, assign());
  }
  return node;
}

// 生成規則: equality = relational ("==" relational | "!=" relational)*
Node *equality() {
  Node *node = relational();

  for (;;) {
    if (consume("==", TK_RESERVED)) {
      node = new_node(ND_EQ, node, relational());
    } else if (consume("!=", TK_RESERVED)) {
      node = new_node(ND_NE, node, relational());
    } else {
      return node;
    }
  }
}

// 生成規則: relational = add ("<" add | "<=" add | ">" add | ">=" add)*
Node *relational() {
  Node *node = add();

  for (;;) {
    if (consume("<", TK_RESERVED)) {
      node = new_node(ND_LT, node, add());
    } else if (consume("<=", TK_RESERVED)) {
      node = new_node(ND_LE, node, add());
    } else if (consume(">", TK_RESERVED)) {
      node = new_node(ND_LT, add(), node);
    } else if (consume(">=", TK_RESERVED)) {
      node = new_node(ND_LE, add(), node);
    } else {
      return node;
    }
  }
}

// 生成規則: add = mul ("+" mul | "-" mul)*
Node *add() {
  Node *node = mul();

  for (;;) {
    if (consume("+", TK_RESERVED)) {
      node = new_node(ND_ADD, node, mul());
    } else if (consume("-", TK_RESERVED)) {
      node = new_node(ND_SUB, node, mul());
    } else {
      return node;
    }
  }
}

// 生成規則: mul = unary ("*" unary | "/" unary)*
Node *mul() {
  Node *node = unary();

  for (;;) {
    if (consume("*", TK_RESERVED)) {
      node = new_node(ND_MUL, node, unary());
    } else if (consume("/", TK_RESERVED)) {
      node = new_node(ND_DIV, node, unary());
    } else {
      return node;
    }
  }
}

// 生成規則: unary = ("+" | "-")? unary
// （X?はXが0回か1回出現する要素を表す）
Node *unary() {
  if (consume("+", TK_RESERVED)) {
    return unary();
  }
  if (consume("-", TK_RESERVED)) {
    return new_node(ND_SUB, new_node_num(0), unary());
  }
  return primary();
}

// 生成規則: primary = "(" expr ")" | num
Node *primary() {
  // 次のトークンが"("なら、"(" expr ")"のはず
  if (consume("(", TK_RESERVED)) {
    Node *node = expr();
    expect(")");
    return node;
  }

  // トークンが識別子かどうか
  Token *tok = consume_ident();
  if (tok) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = ND_LVAR;

    LVar *lvar = find_lvar(tok);
    if (lvar) {
      node->offset = lvar->offset;
    } else {
      // ローカル変数が初めて見つかったものだった場合
      lvar = calloc(1, sizeof(LVar));
      lvar->next = locals;
      lvar->name = tok->str;
      lvar->len = tok->len;
      if (locals == NULL) {
        lvar->offset = 8;
      } else {
        lvar->offset = locals->offset + 8;
      }
      node->offset = lvar->offset;
      locals = lvar;
    }
    return node;
  }

  // そうでなければ数値のはず
  return new_node_num(expect_number());
}

// 指定された変数のアドレスをスタックにpush（rbpからのオフセットで求められる）
void gen_lval(Node *node) {
  if (node->kind != ND_LVAR) {
    error_at(token->str, "代入の左辺値が変数ではありません");
  }

  // rbp（現在の関数の開始位置を表す）の値をraxに入れる
  printf("  mov rax, rbp\n");
  // raxの値をオフセット分だけ引いて、変数のアドレスをraxに入れる
  printf("  sub rax, %d\n", node->offset);
  // 特定したアドレスをスタックにpush
  printf("  push rax\n");
}

int genCounter = 0;

void gen(Node *node) {
  genCounter += 1;
  int id = genCounter;
  switch (node->kind) {
  case ND_BLOCK:
    for (int i = 0; node->block[i]; i++) {
      gen(node->block[i]);
    }
    return;
  case ND_IF:
    gen(node->lhs);
    printf("  pop rax\n");
    printf("  cmp rax, 0\n");
    printf("  je  .Lelse%03d\n", id);
    if (node->rhs->kind == ND_ELSE) {
      gen(node->rhs->lhs);
    } else {
      gen(node->rhs);
    }
    printf("  jmp .Lend%03d\n", id);
    printf(".Lelse%03d:\n", id);
    if (node->rhs->kind == ND_ELSE) {
      gen(node->rhs->rhs);
    }
    printf(".Lend%03d:\n", id);
    return;
  case ND_WHILE:
    printf(".Lbegin%03d:\n", id);
    gen(node->lhs);
    printf("  pop rax\n");
    printf("  cmp rax, 0\n");
    printf("  je  .Lend%03d\n", id);
    gen(node->rhs);
    printf("  jmp  .Lbegin%03d\n", id);
    printf(".Lend%03d:\n", id);
    return;
  case ND_FOR:
    gen(node->lhs->lhs);
    printf(".Lbegin%03d:\n", id);
    gen(node->lhs->rhs);
    printf("  pop rax\n");
    printf("  cmp rax, 0\n");
    printf("  je  .Lend%03d\n", id);
    gen(node->rhs->rhs);
    gen(node->rhs->lhs);
    printf("  jmp  .Lbegin%03d\n", id);
    printf(".Lend%03d:\n", id);
    return;
  case ND_NUM:
    printf("  push %d\n", node->val);
    return;
  case ND_LVAR:
    gen_lval(node);
    printf("  pop rax\n");
    printf("  mov rax, [rax]\n");
    printf("  push rax\n");
    return;
  case ND_ASSIGN:
    gen_lval(node->lhs);
    gen(node->rhs);

    printf("  pop rdi\n");
    printf("  pop rax\n");
    printf("  mov [rax], rdi\n");
    printf("  push rdi\n");
    return;
  case ND_RETURN:
    gen(node->lhs);
    printf("  pop rax\n");
    printf("  mov rsp, rbp\n");
    printf("  pop rbp\n");
    printf("  ret\n");
    return;
  }

  // 以降の処理は2項演算子を処理する（rdiとraxの結果を計算）
  gen(node->lhs);
  gen(node->rhs);

  printf("  pop rdi\n");
  printf("  pop rax\n");

  switch (node->kind) {
  case ND_ADD:
    printf("  add rax, rdi\n");
    break;
  case ND_SUB:
    printf("  sub rax, rdi\n");
    break;
  case ND_MUL:
    printf("  imul rax, rdi\n");
    break;
  case ND_DIV:
    printf("  cqo\n");
    printf("  idiv rdi\n");
    break;
  case ND_EQ:
    printf("  cmp rax, rdi\n");
    printf("  sete al\n");
    printf("  movzb rax, al\n");
    break;
  case ND_NE:
    printf("  cmp rax, rdi\n");
    printf("  setne al\n");
    printf("  movzb rax, al\n");
    break;
  case ND_LT:
    printf("  cmp rax, rdi\n");
    printf("  setl al\n");
    printf("  movzb rax, al\n");
    break;
  case ND_LE:
    printf("  cmp rax, rdi\n");
    printf("  setle al\n");
    printf("  movzb rax, al\n");
    break;
  }

  printf("  push rax\n");
}
