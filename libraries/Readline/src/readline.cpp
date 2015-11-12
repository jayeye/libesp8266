#ifndef _READLINE_H_
#define _READLINE_H_

class Readline {
 public:
  Readline();
  Readline(unsigned int line_length);
  Readline(unsigned int line_length, unsigned int buffered lines);

  bool poll();
  char* readline();
 private:
  unsigned int buf_size_;
  unsigned int line_len_;
  char **buffer_;
};
  
