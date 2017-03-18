#pragma once
void set_text_prop(TextLayer * layer, const char * font, GTextAlignment align, bool white);
char * cut_till(char ** line, char delimiter);