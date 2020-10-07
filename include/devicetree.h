#ifndef _DEVICETREE_H
#define _DEVICETREE_H

#include <stdint.h>

struct fdt_header {
    uint32_t magic;
    uint32_t totalsize;
    uint32_t off_dt_struct;
    uint32_t off_dt_strings;
    uint32_t off_mem_rsvmap;
    uint32_t version;
    uint32_t last_comp_version;
    uint32_t boot_cpuid_phys;
    uint32_t size_dt_strings;
    uint32_t size_dt_struct;
};

struct fdt_reserve_entry {
    uint64_t address;
    uint64_t size;
};

struct fdt_prop_entry {
    uint32_t len;
    uint32_t nameoffset;
};

typedef struct of_property {
    struct of_property *op_next;
    char *              op_name;
    uint32_t            op_length;
    void *              op_value;
    uint8_t             op_storage[];
} of_property_t;

typedef struct of_node {
    struct of_node *on_next;
    struct of_node *on_parent;
    char *          on_name;
    struct of_node *on_children;
    of_property_t * on_properties;
    uint8_t         on_storage[];
} of_node_t;

#define FDT_END         0x00000009
#define FDT_BEGIN_NODE  0x00000001
#define FDT_END_NODE    0x00000002
#define FDT_PROP        0x00000003
#define FDT_NOP         0x00000004

#define FDT_MAGIC       0xd00dfeed

void dt_dump_tree();
of_node_t *dt_parse(void *ptr);
long dt_total_size();
void * dt_fdt_base();
of_node_t *dt_find_node_by_phandle(uint32_t phandle);
of_node_t *dt_find_node(char *key);
of_property_t *dt_find_property(void *key, char *propname);
uint32_t dt_get_property_value_u32(void *key, char *propname, uint32_t def_val, int check_parent);

#endif /* _DEVICETREE_H */
