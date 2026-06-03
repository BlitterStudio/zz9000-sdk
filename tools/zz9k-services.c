/*
 * List SDK v2 services advertised by the firmware service registry.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "zz9k/caps.h"
#include "zz9k/host.h"
#include <stdio.h>
#include <string.h>

static void print_usage(void)
{
  printf("usage: zz9k-services [--all]\n");
}

static void print_flag_name(int *first, const char *name)
{
  printf("%s%s", *first ? "" : ",", name);
  *first = 0;
}

static void print_service_flags(uint32_t service_id, uint32_t flags)
{
  uint32_t remaining;
  uint32_t i;
  uint32_t count;
  int first = 1;

  printf("           flag-names=");
  remaining = flags;
  count = zz9k_known_service_flag_count(service_id);
  for (i = 0; i < count; i++) {
    const char *name;
    uint32_t flag;

    flag = zz9k_known_service_flag(service_id, i);
    if (flag == 0U || (flags & flag) == 0U) {
      continue;
    }

    name = zz9k_service_flag_name(service_id, flag);
    if (name) {
      print_flag_name(&first, name);
      remaining &= ~flag;
    }
  }
  if (remaining != 0U) {
    printf("%s0x%08lx", first ? "" : ",", (unsigned long)remaining);
    first = 0;
  }
  if (first) {
    printf("none");
  }
  printf("\n");
}

static void print_service(const ZZ9KServiceInfo *service,
                          const char *fallback_name)
{
  const char *name;

  name = service->name[0] ? service->name : fallback_name;
  printf("%-10s id=0x%04lx version=%lu.%lu caps=0x%08lx flags=0x%08lx "
         "ops=0x%04lx+%lu inline=%lu\n",
         name,
         (unsigned long)service->service_id,
         (unsigned long)((service->version >> 16) & 0xffffUL),
         (unsigned long)(service->version & 0xffffUL),
         (unsigned long)service->capability_bits,
         (unsigned long)service->flags,
         (unsigned long)service->opcode_base,
         (unsigned long)service->opcode_count,
         (unsigned long)service->max_inline_payload);
  print_service_flags(service->service_id, service->flags);
}

int main(int argc, char **argv)
{
  ZZ9KContext *ctx = 0;
  ZZ9KCaps caps;
  uint32_t count;
  uint32_t i;
  int show_all = 0;
  int status;

  if (argc > 2) {
    print_usage();
    return 1;
  }
  if (argc == 2) {
    if (strcmp(argv[1], "--all") != 0) {
      print_usage();
      return 1;
    }
    show_all = 1;
  }

  status = zz9k_open(&ctx);
  if (status != ZZ9K_STATUS_OK) {
    printf("zz9k-services: open failed: %s (%d)\n",
           zz9k_status_name(status), status);
    return 1;
  }

  memset(&caps, 0, sizeof(caps));
  status = zz9k_query_caps(ctx, &caps);
  if (status != ZZ9K_STATUS_OK) {
    printf("zz9k-services: query caps failed: %s (%d)\n",
           zz9k_status_name(status), status);
    zz9k_close(ctx);
    return 1;
  }

  if ((caps.capability_bits & ZZ9K_CAP_SERVICE_DISCOVERY) == 0) {
    printf("service discovery unavailable\n");
    zz9k_close(ctx);
    return 1;
  }

  count = zz9k_known_service_count();
  for (i = 0; i < count; i++) {
    uint32_t service_id;
    const char *fallback_name;
    ZZ9KServiceInfo service;

    service_id = zz9k_known_service_id(i);
    fallback_name = zz9k_service_name(service_id);
    memset(&service, 0, sizeof(service));

    if (!zz9k_service_advertised_by_caps(service_id,
                                         caps.capability_bits)) {
      if (show_all) {
        printf("%-10s id=0x%04lx skipped (capability not advertised)\n",
               fallback_name, (unsigned long)service_id);
      }
      continue;
    }

    status = zz9k_query_service(ctx, service_id, &service);
    if (status == ZZ9K_STATUS_NOT_FOUND ||
        status == ZZ9K_STATUS_UNSUPPORTED) {
      if (show_all) {
        printf("%-10s id=0x%04lx unavailable: %s (%d)\n",
               fallback_name, (unsigned long)service_id,
               zz9k_status_name(status), status);
      }
      continue;
    }
    if (status != ZZ9K_STATUS_OK) {
      printf("%-10s id=0x%04lx query failed: %s (%d)\n",
             fallback_name, (unsigned long)service_id,
             zz9k_status_name(status), status);
      continue;
    }

    print_service(&service, fallback_name);
  }

  zz9k_close(ctx);
  return 0;
}
