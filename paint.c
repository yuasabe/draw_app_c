#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// array for history recording
const int max_history = 5;
const int bufsize = 1000;

char history[max_history][bufsize];

// Structure for canvas
typedef struct
{
  int width;
  int height;
  char **canvas;
  char pen;
} Canvas;

Canvas *init_canvas(int width,int height, char pen)
{
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

void reset_canvas(Canvas *c)
{
  const int width = c->width;
  const int height = c->height;
  memset(c->canvas[0], ' ', width*height*sizeof(char));
}

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

void free_canvas(Canvas *c)
{
  free(c->canvas[0]); //  for 2-D array free
  free(c->canvas);
  free(c);
}

void rewind_screen(FILE *fp,unsigned int line)
{
  fprintf(fp,"\033[%dA",line);
}

void clear_command(FILE *fp)
{
  fprintf(fp,"\033[2K");
}
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

void save_history(const char *filename, const int history_size)
{
  const char *default_history_file = "history.txt";
  if (filename == NULL)
    filename = default_history_file;
  
  FILE *fp;
  if ((fp = fopen(filename, "w")) == NULL) {
    fprintf(stderr, "error: cannot open %s.\n", filename);
    return;
  }
  
  for (int i = 0; i < history_size; i++) {
    fprintf(fp, "%s", history[i]);
  }

  printf("saved as \"%s\"\n", filename);

  fclose(fp);
}

// Interpret and execute a command
//   return value:
//     0, normal commands such as "line"
//     1, unknown or special commands (not recorded in history[])
//     2, quit
int interpret_command(const char *command, int *hsize, Canvas *c)
{
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
  
  if (strcmp(s, "save") == 0) {
    s = strtok(NULL, " ");
    save_history(s, *hsize);
    return 1;
  }

  if (strcmp(s, "undo") == 0) {
    reset_canvas(c);
    for (int i = 0; i < *hsize - 1; i++) {
      interpret_command(history[i], NULL,c);
    }
    (*hsize)--;
    return 1;
  }

  if (strcmp(s, "quit") == 0) {
    return 2;
  }

  printf("error: unknown command.\n");

  return 1;
}

int main()
{
  const int width = 70;
  const int height = 40;
  char pen = '*';
  
  FILE *fp;
  char buf[bufsize];
  fp = stdout;
  Canvas *c = init_canvas(width,height, pen);
  
  int hsize = 0;  // history size
  while (hsize < max_history) {

    printf("%d > ", hsize);
    fgets(buf, bufsize, stdin);

    const int r = interpret_command(buf, &hsize,c);
    if (r == 2) break;
    if (r == 0) {
      strcpy(history[hsize], buf);
      hsize++;
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
