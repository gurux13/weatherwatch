#pragma once
void set_text_prop(TextLayer * layer, const char * font, GTextAlignment align, bool white);
char * cut_till(char ** line, char delimiter);
int get_conditions_icon_id(char icon);