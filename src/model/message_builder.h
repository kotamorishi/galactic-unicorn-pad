// message_builder.h — holds the in-progress message as the user taps word tiles.
//
// Words are joined with single spaces. The 128-char limit (MESSAGE_MAX_CHARS,
// enforced by the LED board) is checked on every append so the user can never
// build an over-length message.
#pragma once
#include <Arduino.h>

class MessageBuilder {
public:
  // Append a word with an automatic separating space. Returns false (and makes
  // no change) if the result would exceed MESSAGE_MAX_CHARS.
  bool appendWord(const char* w);
  // Append an explicit extra space. Returns false if it would overflow.
  bool appendSpace();
  // Remove the last word (and the space before it). No-op when empty.
  void backspaceToken();
  // Reset to empty.
  void clear();

  const String& text() const { return _text; }
  size_t length() const { return _text.length(); }
  bool empty() const { return _text.length() == 0; }

private:
  String _text;
};
