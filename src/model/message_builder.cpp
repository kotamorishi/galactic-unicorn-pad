#include "message_builder.h"
#include "app_config.h"

bool MessageBuilder::appendWord(const char* w) {
  if (w == nullptr || w[0] == '\0') return true;
  size_t addition = strlen(w) + (_text.length() > 0 ? 1 : 0);  // +1 for the space
  if (_text.length() + addition > MESSAGE_MAX_CHARS) return false;
  if (_text.length() > 0) _text += ' ';
  _text += w;
  return true;
}

bool MessageBuilder::appendSpace() {
  if (_text.length() + 1 > MESSAGE_MAX_CHARS) return false;
  _text += ' ';
  return true;
}

void MessageBuilder::backspaceToken() {
  int n = _text.length();
  // strip trailing spaces
  while (n > 0 && _text[n - 1] == ' ') n--;
  // remove the last word
  while (n > 0 && _text[n - 1] != ' ') n--;
  // strip the separating spaces that preceded it
  while (n > 0 && _text[n - 1] == ' ') n--;
  _text = _text.substring(0, n);
}

void MessageBuilder::clear() {
  _text = "";
}
