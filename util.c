#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <stdint.h>
#include <unistd.h>
#include <dlfcn.h>
#include "citron.h"

char *np;

/**
 * Determines the size of the specified file.
 */
int
fsize (char *filename)
{
  int size;
  FILE *fh;
  fh = fopen (filename, "rb");
  if (fh != NULL)
    {
      if (fseek (fh, 0, SEEK_END))
	{
	  fclose (fh);
	  return -1;
	}
      size = ftell (fh);
      fclose (fh);
      return size;
    }
  return -1;
}

/**
 * DebugTree
 *
 * For debugging purposes, prints the internal AST.
 */
void
ctr_internal_debug_tree (ctr_tnode * ti, int indent)
{
  char *str;
  char *vbuf;
  ctr_tlistitem *li;
  ctr_tnode *t;
  if (indent > 20)
    exit (1);
  li = ti->nodes;
  t = li->node;
  while (1)
    {
      int i;
      for (i = 0; i < indent; i++)
	printf (" ");
      str = ctr_heap_allocate (40 * sizeof (char));
      switch (t->type)
	{
	case CTR_AST_NODE_EXPRASSIGNMENT:
	  str = "ASSIGN";
	  break;
	case CTR_AST_NODE_EXPRMESSAGE:
	  str = "MESSAG";
	  break;
	case CTR_AST_NODE_UNAMESSAGE:
	  str = "UMSSAG";
	  break;
	case CTR_AST_NODE_KWMESSAGE:
	  str = "KMSSAG";
	  break;
	case CTR_AST_NODE_BINMESSAGE:
	  str = "BMSSAG";
	  break;
	case CTR_AST_NODE_LTRSTRING:
	  str = "STRING";
	  break;
	case CTR_AST_NODE_REFERENCE:
	  str = "REFRNC";
	  break;
	case CTR_AST_NODE_LTRNUM:
	  str = "NUMBER";
	  break;
	case CTR_AST_NODE_CODEBLOCK:
	  str = "CODEBL";
	  break;
	case CTR_AST_NODE_RETURNFROMBLOCK:
	  str = "RETURN";
	  break;
	case CTR_AST_NODE_PARAMLIST:
	  str = "PARAMS";
	  break;
	case CTR_AST_NODE_INSTRLIST:
	  str = "INSTRS";
	  break;
	case CTR_AST_NODE_ENDOFPROGRAM:
	  str = "EOPROG";
	  break;
	case CTR_AST_NODE_NESTED:
	  str = "NESTED";
	  break;
	case CTR_AST_NODE_LTRBOOLFALSE:
	  str = "BFALSE";
	  break;
	case CTR_AST_NODE_LTRBOOLTRUE:
	  str = "BLTRUE";
	  break;
	case CTR_AST_NODE_LTRNIL:
	  str = "LTRNIL";
	  break;
  case CTR_AST_NODE_IMMUTABLE:
    str = "IMMUTABLE";
    break;
	default:
	  str = "UNKNW?";
	  break;
	}
      vbuf = ctr_heap_allocate (sizeof (char) * (t->vlen + 1));
      strncpy (vbuf, t->value, t->vlen);
      printf ("%s %s (%p)\n", str, vbuf, (void *) t);
      if (t->nodes)
	ctr_internal_debug_tree (t, indent + 1);
      if (!li->next)
	break;
      li = li->next;
      t = li->node;

    }
}

/**
 * @internal
 * Internal plugin loader.
 * Tries to locate a plugin by its name.
 *
 * In Citron a plugin is loaded automatically as soon as a symbol
 * has been enountered that cannot be found in global scope.
 *
 * Before raising an error Citron will attempt to load a plugin
 * to meet the dependency.
 *
 * To install a plugin, copy the plugin folder to the mods folder
 * in the extensions directory of the interpreter.
 *
 * So, for instance, to install the 'Coffee Percolator Plugin',
 * we copy the libctrpercolator.so in folder mods/percolator/,
 * the resulting path is: mods/percolator/libctrpercolator.so.
 *
 * General path format:
 *
 * mods/X/libctrX.so
 *
 * where X is the name of the object the plugin offers in lowercase.
 *
 * On loading, the plugin will get a chance to add its objects to the world
 * through a constructor function.
 */
typedef void *(*plugin_init_func) ();
void *
ctr_internal_plugin_find (ctr_object * key)
{
  ctr_object *modNameObject = ctr_internal_cast2string (key);
  void *handle;
  char pathNameMod[1024];
  char *modName;
  char *modNameLow;
  plugin_init_func init_plugin;
  char *realPathModName = NULL;
  modName = ctr_heap_allocate_cstring (modNameObject);
  modNameLow = modName;
  for (; *modNameLow; ++modNameLow)
    *modNameLow = tolower (*modNameLow);
  snprintf (pathNameMod, 1024, (CTR_STD_EXTENSION_PATH "/mods/%s/libctr%s.so"), modName, modName);
  ctr_heap_free (modName);
  realPathModName = realpath (pathNameMod, NULL);
  if (access (realPathModName, F_OK) == -1)
    return NULL;
  handle = dlopen (realPathModName, RTLD_NOW);
  if (!handle)
    {
      printf ("%s: %s\n", "Failed to open file", dlerror ());
      return NULL;
    }
  *(void **) (&init_plugin) = dlsym (handle, "begin");
  if (!init_plugin)
    return NULL;
  (void) init_plugin ();
  return handle;
}
