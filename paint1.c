#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <assert.h>

const int bufsize = 1000;

struct history
{
  char *command;
  struct history *next;
};
typedef struct history History;

// Structure for canvas
typedef struct {
  int width;
  int height;
  char **canvas;
  char pen;
} Canvas;

History *undo(History *h, Canvas *c);
int interpret_command(const char *, History *, Canvas *);

History *init_history() {
  History *begin = NULL;
  return begin;
}

int history_size(History *h) {
  int hsize = 0;
  History *p;
  for (p = h ; p != NULL ; p = p->next)
    hsize++;
  return hsize;
}

History *push_history(History *begin, char *command) {
  // Create a new element
  History *new = (History *)malloc(sizeof(History));
  char *c = (char *)malloc(strlen(command) + 1);
  strcpy(c, command);
  new->command = c;
  new->next = NULL;

  if (begin == NULL) return new;

  History *current = begin;
  while(current->next != NULL){
    current = current->next;
  }
  current->next = new;
  return begin;
}

History *undo(History *h, Canvas *c) {
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
    interpret_command(p->command, NULL, c);

    p = q;
    q = p->next;
  }
  // このループを抜けた時点でqは終端だが、pに終端の一つ前が格納されている
  // このpはまだ実行されていないのでまず実行する
  // その後、削除すべきqを処理
  interpret_command(p->command, NULL, c);
  free(q->command);
  free(q); 
  p->next = NULL; //pが次の終端となるように指定

  return h;//
  
}


Canvas *init_canvas(int width,int height, char pen) {
  Canvas *new = (Canvas *)malloc(sizeof(Canvas));
  new->width = width;
  new->height = height;
  new->canvas = (char **)malloc(width * sizeof(char *));

  char *tmp = (char *)malloc(width*height*sizeof(char));
  memset(tmp, ' ', width*height*sizeof(char));
  for (int i = 0 ; i < width ; i++){
    new->canvas[i] = tmp + i * height;
  }

  new->pen = pen;
  return new;
}

void reset_canvas(Canvas *c) {
  const int width = c->width;
  const int height = c->height;
  memset(c->canvas[0], ' ', width*height*sizeof(char));
}

void print_canvas(FILE *fp, Canvas *c) {
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

void free_canvas(Canvas *c) {
  free(c->canvas[0]); //  for 2-D array free
  free(c->canvas);
  free(c);
}

void rewind_screen(FILE *fp,unsigned int line) {
  fprintf(fp,"\033[%dA",line);
}

void clear_command(FILE *fp) {
  fprintf(fp,"\033[2K");
}
void clear_screen(FILE *fp) {
  fprintf(fp, "\033[2J");
}

int max(const int a, const int b) {
  return (a > b) ? a : b;
}

void draw_line(Canvas *c, const int x0, const int y0, const int x1, const int y1) {
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

void draw_rectangle(Canvas *c, const int x0, const int y0, const int x1, const int y1) {
  for (int i = y0; i < y1; i++) {
    draw_line(c,x0,i,x1,i);
  }
}

void draw_circle(Canvas *c, const int x0, const int y0, const int r) {
  const int width = c->width;
  const int height = c->height;
  char pen = c->pen;
  if ( (x0-r >= 0) && (x0+r <= width) && (y0-r >= 0) && (y0+r <= height)) {
    for (int y = y0-2*r; y < y0+2*r; y++) {
      for (int x = x0-3*r; x < x0+3*r; x++) {
        double d = sqrt(pow(x-x0,2)/4 + pow(y-y0,2));
        if ((int) d == r) {
          c->canvas[x][y] = pen;
        }
      }
    }
  }
}

void save_history(const char *filename, History *h) {
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
//     1, unknown or special commands (not recorded in history[])
//     2, quit
int interpret_command(const char *command, History *h, Canvas *c) {
  char buf[bufsize];
  strcpy(buf, command);
  buf[strlen(buf) - 1] = 0; // remove the newline character at the end

  const char *s = strtok(buf, " ");

  // The first token corresponds to command
  if (strcmp(s, "line") == 0) {
    int x0, y0, x1, y1;
    x0 = 0; y0 = 0; x1 = 0; y1 = 0; // initialize
    x0 = atoi(strtok(NULL, " "));
    y0 = atoi(strtok(NULL, " "));
    x1 = atoi(strtok(NULL, " "));
    y1 = atoi(strtok(NULL, " "));
    draw_line(c,x0, y0, x1, y1);
    return 0;
  }

  if (strcmp(s, "rect") == 0) {
    int x0, y0, x1, y1;
    x0 = 0; y0 = 0; x1 = 0; y1 = 0; // initialize
    x0 = atoi(strtok(NULL, " "));
    y0 = atoi(strtok(NULL, " "));
    x1 = atoi(strtok(NULL, " "));
    y1 = atoi(strtok(NULL, " "));
    draw_rectangle(c,x0,y0,x1,y1);
    return 0;
  }

  if (strcmp(s, "circle") == 0) {
    int x0, y0, r;
    x0 = 0; y0 = 0; r = 0;
    x0 = atoi(strtok(NULL, " "));
    y0 = atoi(strtok(NULL, " "));
    r = atoi(strtok(NULL, " "));
    draw_circle(c,x0,y0,r);
    return 0;
  }
  
  if (strcmp(s, "save") == 0) {
    s = strtok(NULL, " ");
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

int main() {
  const int width = 150;
  const int height = 40;
  char pen = '*';
  
  FILE *fp;
  char buf[bufsize];
  fp = stdout;
  Canvas *c = init_canvas(width,height, pen);
  
  int hsize = 0;  // history size

  History *h = init_history();

  print_canvas(fp, c);

  while (1) {
    hsize = history_size(h);

    printf("%d > ", hsize);
    fgets(buf, bufsize, stdin);

    const int r = interpret_command(buf, h,c);
    if (r == 2) break;
    if (r == 0) {
      h = push_history(h, buf);
    }

    print_canvas(fp, c);
    rewind_screen(fp, height+3); // rewind the screen to command input
    clear_command(fp); // clear the previous command
  }

  clear_screen(fp);
  free_canvas(c);
  fclose(fp);

  return 0;
}
