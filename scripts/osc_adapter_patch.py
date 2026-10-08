"""NEXVARY changes to GPL-2.0-or-later OSC sources; distribute with source.
Restrict enumeration before MMIO access, select exactly one hidden AHCI port.
"""
from pathlib import Path


def apply(source):
    path = Path(source) / 'src/opensuperclone/io.c'
    text = path.read_text()
    anchor = '  device_count_ccc = 0;\n  if (direct_mode_ccc)'
    replacement = '''  device_count_ccc = 0;
  /* NEXVARY: never enumerate/probe unrelated controllers in adapter mode. */
  const char *dc_pci = getenv("DC_AHCI_PCI");
  if (dc_pci) {
    if (!ahci_mode_ccc || !getenv("DC_AHCI_PORT") || !getenv("DC_AHCI_SERIAL")) return -1;
    get_device_resources_ccc((char *)dc_pci, "ahci");
    if (device_count_ccc != 1 || device_visable_ccc[0]) return -1;
    identify_device_ahci_ccc(0);
    return 0;
  }
  if (direct_mode_ccc)'''
    assert text.count(anchor) == 1
    text = text.replace(anchor, replacement)
    anchor = '      if (possible_device)\n      {\n        get_device_information_ccc'
    replacement = '''      if (possible_device && (!getenv("DC_AHCI_PORT") ||
          i == atoi(getenv("DC_AHCI_PORT"))))
      {
        get_device_information_ccc'''
    assert text.count(anchor) == 1
    text = text.replace(anchor, replacement)
    anchor = '    if (drive_serial_ccc == NULL && !superclone_ccc)'
    replacement = '''    if (getenv("DC_AHCI_PCI")) {
      if (device_count_ccc != 1 || device_visable_ccc[0] ||
          strcmp(serial_ccc[0], getenv("DC_AHCI_SERIAL")) != 0) {
        fprintf(stderr, "DC_IDENTITY_CHANGED\\n"); return -1;
      }
      choice = 1;
    }
    else if (drive_serial_ccc == NULL && !superclone_ccc)'''
    # Only AHCI branch; IDE behavior stays upstream.
    assert text.count(anchor) >= 2
    text = text.replace(anchor, replacement, 1)
    path.write_text(text)
