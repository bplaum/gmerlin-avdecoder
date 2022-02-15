#!/usr/bin/gawk -f
BEGIN { 
  FS = "\t";
  printf "static struct\n";
  printf "  {\n";
  printf "  char * iso_639_b;\n";
  printf "  char * name;\n";
  printf "  }\n";
  printf "language_codes[] =\n";
  printf "  {\n";
  }
  {
  printf "    { \"%s\", ", $2;
  printf "\"%s\", },\n", $4
  }
END {
  printf "  };\n" }
