/*****
 * interact.cc
 *
 * The glue between the lexical analyzer and the readline library.
 *****/

#include <cstdlib>
#include <cassert>
#include <iostream>
#include <sstream>
#include <sys/wait.h>
#include <unistd.h>
#include "interact.h"

#if defined(HAVE_LIBREADLINE) && defined(HAVE_LIBCURSES)
#include <readline/readline.h>
#include <readline/history.h>
#include <csignal>
#endif

#include "util.h"
#include "errormsg.h"

using std::cout;
using namespace settings;

namespace run {
  void init_readline(bool);
}

namespace interact {

int interactive=false;
bool virtualEOF=true;
bool resetenv;
bool uptodate=true;

completer *currentCompleter=0;

void setCompleter(completer *c) {
  currentCompleter=c;
}

char *call_completer(const char *text, int state) {
  return currentCompleter ? (*currentCompleter)(text, state) : 0;
}

#if defined(HAVE_LIBREADLINE) && defined(HAVE_LIBCURSES)
void init_completion() {
  rl_completion_entry_function=call_completer;

  rl_completion_append_character='\0'; // Don't add a space after a match.

  // Build a string containing all characters that separate words to be
  // completed.  All characters that can't form part of an identifier are
  // treated as break characters.
  static char break_characters[128];
  int j=0;
  for (int c=9; c<128; ++c)
    if (!isalnum(c) && c != '_') {
      break_characters[j]=c;
      ++j;
    }
  break_characters[j]='\0';
  rl_completer_word_break_characters=break_characters;
}
#endif  

void pre_readline()
{
#if defined(HAVE_LIBREADLINE) && defined(HAVE_LIBCURSES)
  run::init_readline(getSetting<bool>("tabcompletion"));
#endif  
}

void init_interactive()
{
#if defined(HAVE_LIBREADLINE) && defined(HAVE_LIBCURSES)
  init_completion();
  read_history(historyname.c_str());
#endif  
}
  
#if !defined(HAVE_LIBREADLINE) || !defined(HAVE_LIBCURSES)
char *readline(const char *prompt) {
  std::cout << prompt;
  string s;
  getline(std::cin,s);
  char *p=(char *) malloc(s.size()+1);
  return strcpy(p,s.c_str());
}
#endif  
  
/* Read a string, and return a pointer to it. Returns NULL on EOF. */
const char *rl_gets()
{
  static char *line_read=NULL;
  /* If the buffer has already been allocated,
     return the memory to the free pool. */
  if(line_read) {
    free(line_read);
    line_read=NULL;
  }
     
  
  // Rebind tab key, as the setting tabcompletion may be changed at runtime.
  pre_readline();

  /* Get a line from the user. */
  while((line_read=readline(getSetting<mem::string>("prompt").c_str()))) {
    if(*line_read == 0) continue;    
    static int pid=0, status=0;
    static bool restart=true;
    if(strcmp(line_read,"help") == 0 || strcmp(line_read,"help;") == 0) {
      if(pid) restart=(waitpid(pid, &status, WNOHANG) == pid);
      if(restart) {
	ostringstream cmd;
	cmd << "'" << getSetting<mem::string>("pdfviewer") << "' " 
	    << docdir << "/asymptote.pdf";
	status=System(cmd,0,false,"pdfviewer","your PDF viewer",&pid);
      }
      continue;
    }
    break;
  }
     
  if(line_read) {
    if(strcmp(line_read,"q") == 0 || strcmp(line_read,"quit") == 0
       || strcmp(line_read,"quit;") == 0
       || strcmp(line_read,"exit") == 0
       || strcmp(line_read,"exit;") == 0)
      return NULL;
  
    /* If the line has any text in it, save it on the history. */
#if defined(HAVE_LIBREADLINE) && defined(HAVE_LIBCURSES)
    if(*line_read) add_history(line_read);
#endif    
  } else {
    cout << endl;
    return "\n";
  }
  
  return line_read;
}

void overflow()
{
  cerr << "warning: buffer overflow, input discarded." << endl;
}

void add_input(char *&dest, const char *src, size_t& size)
{
  
  size_t len=strlen(src);
  if(len == 0) return;
  
  if(len >= size) {overflow(); return;}
  
  strcpy(dest,src);
  // Auto-terminate each line:
  if(dest[len-1] != ';') {dest[len]=';'; len++;}
  
  size -= len;
  dest += len;
}
 
static const char *input="input "; 
static const char *inputexpand="erase(); include ";
static size_t ninput=strlen(input);
static size_t ninputexpand=strlen(inputexpand);
  
size_t interactive_input(char *buf, size_t max_size)
{
  static bool inputmode=false;
    
  assert(max_size > 0);
  size_t size=max_size-1;
  char *to=buf;
    
  if(virtualEOF) return 0;
  
  static const char *line;

  if(inputmode) {
    inputmode=false;  
    virtualEOF=true;
    line += ninput;
    strcpy(to,inputexpand);
    to += ninputexpand;
    add_input(to,line,size);
    return to-buf;
  }
  
  if(em->errors())
    em->clear();
  
  ShipoutNumber=0;
  
  if((line=rl_gets())) {
    errorstream::interrupt=false;
    virtualEOF=true;
    
    if(strncmp(line,input,ninput) == 0) {
      inputmode=true;
      resetenv=true;
      return 0;
    }
    
    if(strcmp(line,"reset") == 0 || strcmp(line,"reset;") == 0) {
      resetenv=true;
      return 0;
    }
    
    add_input(to,line,size);
    return to-buf;
  } else {
#if defined(HAVE_LIBREADLINE) && defined(HAVE_LIBCURSES)
    stifle_history(getSetting<int>("historylines"));
    write_history(historyname.c_str());
#endif    
    return 0;
  }
}

} // namespace interact
