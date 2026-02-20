#include "wifi_profile_nvm.h"

#include "cy_pdl.h"
#include "cybsp.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define WIFI_PROFILE_NVM_MAGIC (0x57465031UL) /* "WFP1" */
#define WIFI_PROFILE_NVM_VERSION (1U)
#define WIFI_PROFILE_NVM_VALID (1U)
#define WIFI_PROFILE_NVM_USER_START ((uint32_t)CYMEM_CM33_0_user_nvm_START)
#define WIFI_PROFILE_NVM_USER_SIZE ((uint32_t)CYMEM_CM33_0_user_nvm_SIZE)
#define WIFI_PROFILE_NVM_SLOT_SIZE (256U)
#define WIFI_PROFILE_NVM_PRIMARY_ADDR (WIFI_PROFILE_NVM_USER_START + WIFI_PROFILE_NVM_USER_SIZE - WIFI_PROFILE_NVM_SLOT_SIZE)
#define WIFI_PROFILE_NVM_LEGACY_ADDR (WIFI_PROFILE_NVM_USER_START)

typedef struct
{
  uint32_t magic;
  uint16_t version;
  uint16_t payload_len;
  uint32_t crc32;
  uint8_t valid;
  uint8_t reserved[3];
  char ssid[WIFI_SSID_MAX_LEN + 1U];
  char password[64U + 1U];
  uint32_t security;
} wifi_profile_record_t;

typedef char wifi_profile_record_size_check[(sizeof(wifi_profile_record_t) <= WIFI_PROFILE_NVM_SLOT_SIZE) ? 1 : -1];

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, uint32_t len)
{
  uint32_t i;
  uint32_t j;

  if (NULL == data)
  {
    return crc;
  }

  for (i = 0U; i < len; i++)
  {
    crc ^= (uint32_t)data[i];
    for (j = 0U; j < 8U; j++)
    {
      if ((crc & 1U) != 0U)
      {
        crc = (crc >> 1U) ^ 0xEDB88320UL;
      }
      else
      {
        crc >>= 1U;
      }
    }
  }
  return crc;
}

static uint32_t profile_crc32(const wifi_profile_record_t *rec)
{
  uint32_t crc;

  if (NULL == rec)
  {
    return 0U;
  }

  crc = 0xFFFFFFFFUL;
  crc = crc32_update(crc, (const uint8_t *)rec->ssid, sizeof(rec->ssid));
  crc = crc32_update(crc, (const uint8_t *)rec->password, sizeof(rec->password));
  crc = crc32_update(crc, (const uint8_t *)&rec->security, sizeof(rec->security));
  return ~crc;
}

static bool read_slot(uint32_t addr, uint8_t *out)
{
  cy_en_rram_status_t st;

  if (NULL == out)
  {
    return false;
  }

  st = Cy_RRAM_TSReadByteArray(RRAMC0, addr, out, WIFI_PROFILE_NVM_SLOT_SIZE);
  if (CY_RRAM_SUCCESS != st)
  {
    (void)printf("[CM33][NVM] Read failed st=%ld addr=0x%08lX len=%lu\n",
                 (long)st,
                 (unsigned long)addr,
                 (unsigned long)WIFI_PROFILE_NVM_SLOT_SIZE);
  }
  return (CY_RRAM_SUCCESS == st);
}

static bool write_slot(uint32_t addr, const uint8_t *in)
{
  cy_en_rram_status_t st;

  if (NULL == in)
  {
    return false;
  }

  st = Cy_RRAM_NvmWriteByteArray(RRAMC0, addr, (uint8_t *)in, WIFI_PROFILE_NVM_SLOT_SIZE);
  if (CY_RRAM_SUCCESS != st)
  {
    (void)printf("[CM33][NVM] Write failed st=%ld addr=0x%08lX\n",
                 (long)st,
                 (unsigned long)addr);
  }
  return (CY_RRAM_SUCCESS == st);
}

static void make_record(wifi_profile_record_t *rec, const ipc_wifi_connect_request_t *profile)
{
  if ((NULL == rec) || (NULL == profile))
  {
    return;
  }

  (void)memset(rec, 0, sizeof(*rec));
  rec->magic = WIFI_PROFILE_NVM_MAGIC;
  rec->version = WIFI_PROFILE_NVM_VERSION;
  rec->payload_len = (uint16_t)sizeof(ipc_wifi_connect_request_t);
  rec->valid = WIFI_PROFILE_NVM_VALID;
  rec->security = profile->security;
  (void)strncpy(rec->ssid, profile->ssid, sizeof(rec->ssid) - 1U);
  (void)strncpy(rec->password, profile->password, sizeof(rec->password) - 1U);
  rec->ssid[sizeof(rec->ssid) - 1U] = '\0';
  rec->password[sizeof(rec->password) - 1U] = '\0';
  rec->crc32 = profile_crc32(rec);
}

static bool parse_record(const wifi_profile_record_t *rec, ipc_wifi_connect_request_t *out_profile, uint32_t addr)
{
  uint32_t expected_crc;

  if ((NULL == out_profile) || (NULL == rec))
  {
    return false;
  }

  if (0xFFFFFFFFUL == rec->magic)
  {
    return false;
  }

  if ((WIFI_PROFILE_NVM_MAGIC != rec->magic) ||
      (WIFI_PROFILE_NVM_VERSION != rec->version) ||
      (WIFI_PROFILE_NVM_VALID != rec->valid) ||
      ((uint16_t)sizeof(ipc_wifi_connect_request_t) != rec->payload_len))
  {
    (void)printf("[CM33][NVM] Invalid profile header at 0x%08lX magic=0x%08lX ver=%u valid=%u len=%u\n",
                 (unsigned long)addr,
                 (unsigned long)rec->magic,
                 (unsigned int)rec->version,
                 (unsigned int)rec->valid,
                 (unsigned int)rec->payload_len);
    return false;
  }

  expected_crc = profile_crc32(rec);
  if (expected_crc != rec->crc32)
  {
    (void)printf("[CM33][NVM] CRC mismatch at 0x%08lX expected=0x%08lX actual=0x%08lX\n",
                 (unsigned long)addr,
                 (unsigned long)expected_crc,
                 (unsigned long)rec->crc32);
    return false;
  }

  (void)memset(out_profile, 0, sizeof(*out_profile));
  (void)strncpy(out_profile->ssid, rec->ssid, sizeof(out_profile->ssid) - 1U);
  (void)strncpy(out_profile->password, rec->password, sizeof(out_profile->password) - 1U);
  out_profile->security = rec->security;
  out_profile->ssid[sizeof(out_profile->ssid) - 1U] = '\0';
  out_profile->password[sizeof(out_profile->password) - 1U] = '\0';
  return true;
}

static bool load_from_addr(uint32_t addr, ipc_wifi_connect_request_t *out_profile)
{
  uint8_t block[WIFI_PROFILE_NVM_SLOT_SIZE];
  wifi_profile_record_t rec;

  if (NULL == out_profile)
  {
    return false;
  }

  if (!read_slot(addr, block))
  {
    return false;
  }

  (void)memset(&rec, 0, sizeof(rec));
  (void)memcpy(&rec, block, sizeof(rec));
  return parse_record(&rec, out_profile, addr);
}

bool wifi_profile_nvm_load(ipc_wifi_connect_request_t *out_profile)
{
  if (NULL == out_profile)
  {
    return false;
  }

  if (load_from_addr(WIFI_PROFILE_NVM_PRIMARY_ADDR, out_profile))
  {
    return true;
  }

  if ((WIFI_PROFILE_NVM_PRIMARY_ADDR != WIFI_PROFILE_NVM_LEGACY_ADDR) &&
      load_from_addr(WIFI_PROFILE_NVM_LEGACY_ADDR, out_profile))
  {
    (void)printf("[CM33][NVM] Loaded profile from legacy address 0x%08lX\n",
                 (unsigned long)WIFI_PROFILE_NVM_LEGACY_ADDR);
    return true;
  }

  return false;
}

bool wifi_profile_nvm_save(const ipc_wifi_connect_request_t *profile)
{
  uint8_t block[WIFI_PROFILE_NVM_SLOT_SIZE];
  uint8_t verify_block[WIFI_PROFILE_NVM_SLOT_SIZE];
  wifi_profile_record_t rec;

  if (NULL == profile)
  {
    return false;
  }

  (void)memset(block, 0xFF, sizeof(block));
  make_record(&rec, profile);

  (void)memcpy(block, &rec, sizeof(rec));
  if (!write_slot(WIFI_PROFILE_NVM_PRIMARY_ADDR, block))
  {
    return false;
  }

  if (!read_slot(WIFI_PROFILE_NVM_PRIMARY_ADDR, verify_block))
  {
    return false;
  }

  if (0 != memcmp(verify_block, block, sizeof(block)))
  {
    (void)printf("[CM33][NVM] Verify mismatch after write at 0x%08lX\n",
                 (unsigned long)WIFI_PROFILE_NVM_PRIMARY_ADDR);
    return false;
  }

  return true;
}

bool wifi_profile_nvm_clear(void)
{
  uint8_t block[WIFI_PROFILE_NVM_SLOT_SIZE];
  bool ok_primary;
  bool ok_legacy = true;

  (void)memset(block, 0xFF, sizeof(block));
  ok_primary = write_slot(WIFI_PROFILE_NVM_PRIMARY_ADDR, block);
  if (WIFI_PROFILE_NVM_PRIMARY_ADDR != WIFI_PROFILE_NVM_LEGACY_ADDR)
  {
    ok_legacy = write_slot(WIFI_PROFILE_NVM_LEGACY_ADDR, block);
  }
  return (ok_primary && ok_legacy);
}
