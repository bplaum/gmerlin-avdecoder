#!/usr/bin/gawk -f
BEGIN { 
  FS = "\t";
  printf "static struct\n";
  printf "  {\n";
  printf "  char * iso_639_t;\n";
  printf "  char * iso_639_b;\n";
  printf "  char * iso_639_2;\n";
  printf "  char * name;\n";
  printf "  char * family;\n";
  printf "  }\n";
  printf "language_codes[] =\n";
  printf "  {\n";
  }
  {
  printf "    { \"%s\", \"%s\", ", $1, $2;
  if(length($3) == 0)
    printf("(char*)0, ");
  else
    printf "\"%s\", ", $3;
  printf "\"%s\", ", $4

  if(length($5) == 0)
    printf("(char*)0 },\n");
  else
    printf "\"%s\" },\n", $5;

  }
END {
  printf "  };\n" }
