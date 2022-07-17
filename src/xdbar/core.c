#include "core.h"
#include <stdio.h>

void usage(void)
{
  puts("USAGE: " NAME " [FLAGS|OPTIONS]");
  puts("FLAGS:");
  puts("  -h    print this help message.");
  puts("  -v    print version.");
  puts("OPTIONS:");
  puts("  -c <config_file>");
  puts("        filepath for runtime configs (supports: lua).");
}
