/*
 * Release capability policy checks for zz9k-services.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define ZZ9K_SERVICES_NO_MAIN 1
#include "../tools/zz9k-services.c"

#include <stdio.h>

/* mhizz9000.library refuses to allocate a decoder unless the firmware
 * advertises AUDIO_STREAM_DRAIN, and Zorro 2 MHI/mpega staging needs the
 * HOST_WINDOW_HEAP allocator. A release image missing either one fails at
 * run time with no diagnostic of its own, so --check-release has to say so. */
static int test_release_requires_mhi_capabilities(void)
{
  uint32_t required;

  required = release_required_capabilities();

  if (!zz9k_has_capability(required, ZZ9K_CAP_AUDIO_STREAM_DRAIN)) {
    printf("release requirement omits AUDIO_STREAM_DRAIN\n");
    return 1;
  }
  if (!zz9k_has_capability(required, ZZ9K_CAP_HOST_WINDOW_HEAP)) {
    printf("release requirement omits HOST_WINDOW_HEAP\n");
    return 2;
  }

  return 0;
}

/* The firmware advertises both bits in the global capability word only --
 * its per-service registry entries carry neither (audio reports
 * AUDIO_DECODE|AUDIO_PLAYBACK, memory reports SHARED_ALLOC|MEMORY_OPS).
 * Requiring them per service would report them missing on correct
 * firmware, so they must stay out of release_services[]. */
static int test_service_requirements_stay_global_only(void)
{
  const uint32_t global_only =
    ZZ9K_CAP_AUDIO_STREAM_DRAIN | ZZ9K_CAP_HOST_WINDOW_HEAP;
  uint32_t i;

  for (i = 0U;
       i < (uint32_t)(sizeof(release_services) / sizeof(release_services[0]));
       i++) {
    if ((release_services[i].required_caps & global_only) != 0U) {
      printf("service 0x%04lx requires a global-only capability\n",
             (unsigned long)release_services[i].service_id);
      return 1;
    }
  }

  return 0;
}

/* The whole point is a reader-friendly report: print_capability_names()
 * falls back to raw hex for bits with no name. */
static int test_missing_capabilities_report_by_name(void)
{
  uint32_t required;
  uint32_t pre_drain_firmware;
  uint32_t missing;

  required = release_required_capabilities();
  pre_drain_firmware =
    required & ~(ZZ9K_CAP_AUDIO_STREAM_DRAIN | ZZ9K_CAP_HOST_WINDOW_HEAP);

  missing = zz9k_missing_capabilities(pre_drain_firmware, required);
  if (missing !=
      (ZZ9K_CAP_AUDIO_STREAM_DRAIN | ZZ9K_CAP_HOST_WINDOW_HEAP)) {
    printf("unexpected missing set 0x%08lx\n", (unsigned long)missing);
    return 1;
  }

  if (!zz9k_capability_name(ZZ9K_CAP_AUDIO_STREAM_DRAIN)) {
    printf("AUDIO_STREAM_DRAIN has no printable name\n");
    return 2;
  }
  if (!zz9k_capability_name(ZZ9K_CAP_HOST_WINDOW_HEAP)) {
    printf("HOST_WINDOW_HEAP has no printable name\n");
    return 3;
  }

  return 0;
}

int main(void)
{
  int rc;

  rc = test_release_requires_mhi_capabilities();
  if (rc != 0) {
    printf("test_release_requires_mhi_capabilities failed (%d)\n", rc);
    return 1;
  }

  rc = test_service_requirements_stay_global_only();
  if (rc != 0) {
    printf("test_service_requirements_stay_global_only failed (%d)\n", rc);
    return 1;
  }

  rc = test_missing_capabilities_report_by_name();
  if (rc != 0) {
    printf("test_missing_capabilities_report_by_name failed (%d)\n", rc);
    return 1;
  }

  printf("services_release_caps_test ok\n");
  return 0;
}
