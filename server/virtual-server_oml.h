/*
 * This file was automatically generated by oml2-scaffold V2.11.0
 * for virtual-server version 1.0.0.
 * Please do not edit.
 */

#ifndef VIRTUAL_SERVER_OML_H
#define VIRTUAL_SERVER_OML_H

#ifdef __cplusplus
extern "C" {
#endif

/* Define HUGE, etc.. */
#define _GNU_SOURCE
#include <math.h>
/* Get size_t and NULL from <stddef.h> */
#include <stddef.h>

#include <oml2/omlc.h>

typedef struct {
  OmlMP *app_metrics;

} oml_mps_t;


  //#ifdef OML_FROM_MAIN
/*
 * Only declare storage once, usually from the main
 * source, where OML_FROM_MAIN is defined
 */

static OmlMPDef oml_app_metrics_def[] = {
  {"GUID", OML_STRING_VALUE},
  {"load", OML_DOUBLE_VALUE},
  {NULL, (OmlValueT)0}
};

static oml_mps_t g_oml_mps_storage;
oml_mps_t* g_oml_mps_virtual_server = &g_oml_mps_storage;

static inline void
oml_register_mps(void)
{
  g_oml_mps_virtual_server->app_metrics = omlc_add_mp("app_metrics", oml_app_metrics_def);

}

  //#else
/*
 * Not included from the main source, only declare the global pointer
 * to the MPs and injection helpers.
 */

//extern oml_mps_t* g_oml_mps_virtual_server;

//#endif /* OML_FROM_MAIN */

static inline int
oml_inject_app_metrics(OmlMP *mp, const char *GUID, double load)
{
  int ret = -1;

  OmlValueU v[2];
  omlc_zero_array(v, 2);

  omlc_set_string(v[0], GUID);
  omlc_set_double(v[1], load);

  ret = omlc_inject(mp, v);

  omlc_reset_string(v[0]);
  return ret;
}


/* Compatibility with old applications */
#ifndef g_oml_mps
# define g_oml_mps	g_oml_mps_virtual_server
#endif

#ifdef __cplusplus
}
#endif

#endif /* VIRTUAL_SERVER_OML_H */
