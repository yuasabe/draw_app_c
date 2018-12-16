#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

const int bufsize = 1000; // 各関数で使用する文字配列バッファのサイズ (サンプルで書いていませんでした、申し訳ない...）
// コマンド履歴を保存する為の線形リスト
struct history
{
  char *command;
  struct history *next;
};
typedef struct history History;

// Structure for canvas
typedef struct
{
  int width;
  int height;
  char **canvas;
  char pen;
} Canvas;

// 二つ以上の構造体や関数が互いに関連してしまっているので、
// プロトタイプ宣言を書いておく
int hitory_size(History *h);
History *push_history(History *h, const char *command);
History *undo(History *h, Canvas *c);

// キャンバス構造体の初期化関数
// 入力
// キャンバスの幅/高さ (int width, int height)
// ペン先の文字 (char pen)
// 出力
// mallocで確保されたキャンバス構造体へのポインタアドレス

Canvas *init_canvas(int width,int height, char pen)
{
  Canvas *new = (Canvas *)malloc(sizeof(Canvas));
  new->width = width;
  new->height = height;

  // 2次元配列の外側をmallocする
  new->canvas = (char **)malloc(width * sizeof(char *));

  // 2次元配列のそれぞれに代入する1次元配列をまとめて確保し、memsetで初期化
  char *tmp = (char *)malloc(width*height*sizeof(char));
  memset(tmp, ' ', width*height*sizeof(char));
  // 外側のインデックスのそれぞれについて、該当するアドレスを代入する
  for (int i = 0 ; i < width ; i++){
    new->canvas[i] = tmp + i * height;
  }
  
  new->pen = pen;
  return new;
}

// キャンバスをスペースで埋める
// 入力
// キャンバスの構造体
// 出力
// なし
// キャンバス構造体中のc->canvas がスペースで埋まる

void reset_canvas(Canvas *c)
{
  const int width = c->width;
  const int height = c->height;
  memset(c->canvas[0], ' ', width*height*sizeof(char));
}

// キャンバスを描画する関数
// 入力
// -FILE *fp: 描画先のファイルポインタ (標準出力したい場合はfp = stdout; とする)
// -Canvas *c: 描画したいキャンバス構造体
// 出力
// なし

void print_canvas(FILE *fp, Canvas *c)
{
  const int height = c->height;
  const int width = c->width;
  char **canvas = c->canvas;
  
  // 上の壁
  fprintf(fp,"+");
  for (int x = 0 ; x < width ; x++)
    fprintf(fp, "-");
  fprintf(fp, "+\n");

  // 外壁と内側
  for (int y = 0 ; y < height ; y++) {
    fprintf(fp,"|");
    for (int x = 0 ; x < width; x++){
      const char c = canvas[x][y];
      fputc(c, fp);
    }
    fprintf(fp,"|\n");
  }
  
  // 下の壁
  fprintf(fp, "+");
  for (int x = 0 ; x < width ; x++)
    fprintf(fp, "-");
  fprintf(fp, "+\n");
  fflush(fp);
}

// キャンバス構造体のfree関数
void free_canvas(Canvas *c)
{
  free(c->canvas[0]);
  free(c->canvas);
  free(c);
}

// ANSIエスケープシーケンスを使ってスクリーンを指定行数巻き戻す
void rewind_screen(FILE *fp,unsigned int line)
{
  fprintf(fp,"\033[%dA",line);
}

// ANSIエスケープシーケンスを使って1行削除 (コマンド部分を消去する為)
void clear_command(FILE *fp)
{
  fprintf(fp,"\033[2K");
}

// ANSIエスケープシーケンスを使って画面をクリア(プログラム終了直前に実行)
void clear_screen(FILE *fp)
{
  fprintf(fp, "\033[2J");
}

int max(const int a, const int b)
{
  return (a > b) ? a : b;
}

void draw_line(Canvas *c, const int x0, const int y0, const int x1, const int y1)
{
  const int width = c->width;
  const int height = c->height;
  char pen = c->pen;
  
  const int n = max(abs(x1 - x0), abs(y1 - y0));
  c->canvas[x0][y0] = pen;
  for (int i = 1; i <= n; i++) {
    const int x = x0 + i * (x1 - x0) / n;
    const int y = y0 + i * (y1 - y0) / n;
    if ( (x >= 0) && (x<width) && (y >= 0) && (y < height))
      c->canvas[x][y] = pen;
  }
}

// 受け取ったファイル名にコマンドラインを保存する
void save_history(const char *filename, History *h)
{
  const char *default_history_file = "history.txt";
  if (filename == NULL)
    filename = default_history_file;
  FILE *fp;
  if ((fp = fopen(filename, "w")) == NULL) {
    fprintf(stderr, "error: cannot open %s.\n", filename);
    return;
  }
  for (History *p = h ; p != NULL ; p = p->next){
    fprintf(fp,"%s",p->command);
  }
  printf("saved as \"%s\"\n", filename);

  fclose(fp);
}

// Interpret and execute a command
//   return value:
//     0, normal commands such as "line"
//     1, unknown or special commands (not recorded in History)
//     2, quit
//     3, If History becomes NULL
//     (undo の結果コマンドリストが全てfreeされたことを知らせる必要がある)

int interpret_command(const char *command, Canvas *c, History *h)
{
  char buf[bufsize];
  strcpy(buf, command);
  buf[strlen(buf) - 1] = 0; // remove the newline character at the end
  
  const char *s = strtok(buf, " ");
      
  // The first token corresponds to command
  if (strcmp(s, "line") == 0) {
    int x0, y0, x1, y1;
    x0 = 0; y0 = 0; x1 = 0; y1 = 0; // initialize
    // 初期実装では4つ全ての数字を入力しないとセグメンテーションフォルトする
    //x0 = atoi(strtok(NULL, " "));
    //y0 = atoi(strtok(NULL, " "));
    //x1 = atoi(strtok(NULL, " "));
    //y1 = atoi(strtok(NULL, " "));
    char *b[4];
    for (int i = 0 ; i < 4; i++){
      b[i] = strtok(NULL, " ");
      if (b[i] == NULL){
	printf("the number of point is not enough.\n");
	return 1;
      }
    }
    x0 = atoi(b[0]);
    y0 = atoi(b[1]);
    x1 = atoi(b[2]);
    y1 = atoi(b[3]);
    
    draw_line(c,x0, y0, x1, y1);
    
    return 0;
  }
  
  if (strcmp(s, "save") == 0) {
    s = strtok(NULL, " ");//第二引数の文字列を受け取る (保存するファイル名)
    save_history(s, h);
    return 1;
  }

  if (strcmp(s, "undo") == 0) {
    reset_canvas(c);
    // do implementation of iterative interpret_command
    //
    h = undo(h,c);
    if (h == NULL){
      return 3;
    }
    return 1;
  }

  if (strcmp(s, "quit") == 0) {
    return 2;
  }

  printf("error: unknown command.\n");

  return 1;
}

// 以下線形リストに対する関数を実装する
// * int history_size(History *h)
// 

//
// 線形リストの要素数を返す関数
// 入力:
// 線形リストの先頭ポインタ
// 出力:
// 要素数 (int)
int history_size(History *h)
{
  int hsize = 0;
  History *p;
  // 先頭から線形リストを順次たどる。その度にカウンタを増やす
  for (p = h ; p != NULL ; p = p->next)
    hsize++;
  return hsize;
}

// 線形リストにコマンド履歴をpushする関数
// (方針としてはpush_back方式とする)
// *push_frontの場合、履歴を全て再実行する場合に順序的に不便
// 入力:
//  現在の線形リストの先頭ポインタ (History* h)
//  追加するコマンド履歴 (const char *command)
// 出力:
//  線形リストの先頭ポインタ(0からの新規追加時のは新しいもの、それ以外は入力と同じ)
//  
History *push_history(History *h, const char *command)
{
  // ひとまず追加する要素を確保
  History *new = (History*)malloc(sizeof(History));
  new->command = (char*)malloc(strlen(command) + 1);
  strcpy(new->command, command);
  new->next = NULL;

  // 現在なにもない場合は新しく作ったものを返せばよい
  if (h == NULL) return new;

  History *current = h;
  while(current->next != NULL){
    current = current->next;
  }
  // 現在の終端ノードのnextに確保した新しいもののアドレスを入れておく
  current->next = new;
  return h; // 返すものは現在の先頭
}

// コマンドをundoする関数
// 入力:
//  コマンド履歴の線形リストのポインタ (History *h)
//  キャンバスをあらわす構造体 (Canvas *c)
// 出力:
//  コマンド履歴の線形リスト (History *h)
//     (コマンドが残っていればその先頭が、なければNULLがかえってくる)
//
//
// ただし、実行として内部で末尾以外のコマンドをinterpret_command キャンバスを更新
// 線形リストの末尾のコマンドが削除されるものとする

History *undo(History *h, Canvas *c)
{
  // 基本的にはpop_back を利用する。ただしループが進むついでにinterpret_command する

  // 現状履歴がないなら何もする必要がない
  // "No history" とでも表示する...
  if (h == NULL) {
    printf("No history\n");
    return NULL;
  }

  History *p = h;
  History *q = h->next;
  // 現在描画コマンド一つであれば、それを実行する必要はないのでそのまま消して終わり
  if (q == NULL){
    free(p->command);
    free(p);
    return NULL;
  }

  // q->next が NULLまでポインタを順次たどる
  while(q->next != NULL){
    // 更新作業のまえにinterpret_commandする (キャンバス更新)
    // 線形リストに記録されているコマンドでは履歴は操作されないので、NULLとしておく
    // interpret_command 内にも undo があるが問題ない（これを再帰関数という）
    interpret_command(p->command, c, NULL);

    p = q;
    q = p->next;
  }
  // このループを抜けた時点でqは終端だが、pに終端の一つ前が格納されている
  // このpはまだ実行されていないのでまず実行する
  // その後、削除すべきqを処理
  interpret_command(p->command, c, NULL);
  free(q->command);
  free(q); 
  p->next = NULL; //pが次の終端となるように指定

  return h;//
  
}

int main()
{
  const int width = 70;
  const int height = 40;
  char pen = '*';
  
  FILE *fp;
  char buf[bufsize];
  fp = stdout;
  Canvas *c = init_canvas(width,height,pen);

  History *h;
  // do something for h
  h = NULL;

  int hsize = 0;
  // while(1) は無限ループをする際の常套句
  // C言語では条件の評価は0でないかどうかなので
  while (1) {
    // 配列版ではhsizeが最新履歴のエントリのindexを記録していた
    // 線形リストの場合、これはリストを終端まで数えた時の要素数になる
    hsize = history_size(h); // 現在の線形リストの要素数を返す関数
    printf("%d > ", hsize);
    if(fgets(buf, bufsize, stdin)==NULL) break; // 追記: Ctrl+D への対応
    if(strlen(buf) == 1) continue; // 追記: 改行のみ叩かれたら先頭に戻る
    const int r = interpret_command(buf,c,h);

    if (r == 2) break;
    if (r == 0) {
      // do some thing for recording history
      h = push_history(h, buf);
    }
    // r == 3 は追加仕様だが、引数で与えたHistoryの線形リストが空になったことを捕まえる
    // interpret_command 自体ではhに格納されているアドレスそのものは書き換わらず
    // 次にこれにアクセスするのは危険なのでこのようにする (すでにfreeされている)
    if (r == 3) h = NULL;

    print_canvas(fp, c);
    rewind_screen(fp, height+3); // rewind the screen to command input
    clear_command(fp); // clear the previous command
  }

  clear_screen(fp);
  free_canvas(c);
  fclose(fp);

  return 0;
}
