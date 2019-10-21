#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "ARM.h"
#include "support_rpi.h"
#include "devicetree.h"
#include "tlsf.h"

#define D(x)

of_node_t *root = NULL;
uint32_t *data;
char *strings;

of_node_t * dt_build_node(of_node_t *parent)
{
    of_node_t *e = tlsf_malloc(tlsf, sizeof(of_node_t));

    if (e != NULL)
    {
        if (parent != NULL)
        {
            e->on_next = parent->on_children;
            parent->on_children = e;
        }
        e->on_children = NULL;
        e->on_properties = NULL;
        e->on_name = (char *)data;
        data += (strlen((char *)data) + 4) / 4;
        uint32_t tmp;

        D(kprintf("[BOOT] new node %s\n", e->on_name));

        while(1)
        {
            D(kprintf("[BOOT] data=%08x @ %08x\n", data, *data));
            switch (tmp = BE32(*data++))
            {
                case FDT_BEGIN_NODE:
                {
                    dt_build_node(e);
                    break;
                }

                case FDT_PROP:
                {
                    of_property_t *p = tlsf_malloc(tlsf, sizeof(of_property_t));
                    p->op_length = BE32(*data++);
                    p->op_name = &strings[BE32(*data++)];
                    if (p->op_length)
                        p->op_value = data;
                    else
                        p->op_value = NULL;
                    p->op_next = e->on_properties;
                    e->on_properties = p;
                    data += (p->op_length + 3)/4;
                    D(kprintf("[BOOT] prop %s with length %d\n", p->op_name, p->op_length));
                    break;
                }

                case FDT_NOP:
                    break;

                case FDT_END_NODE:
                    return e;

                default:
                    D(kprintf("[BOOT] unknown node %08x\n", tmp));
            }
        }
    }
    return e;
}

static struct fdt_header *hdr;

long dt_total_size()
{
    if (hdr != NULL)
        return BE32(hdr->totalsize);
    else
        return 0;
}

of_node_t * dt_parse(void *dt)
{
    uint32_t token = 0;

    hdr = dt;

    D(kprintf("[BOOT] Checking device tree at %08x\n", hdr));
    D(kprintf("[BOOT] magic=%08x\n", BE32(hdr->magic)));

    if (hdr->magic == BE32(FDT_MAGIC))
    {
        D(kprintf("[BOOT] Appears to be a valid device tree\n"));
        D(kprintf("[BOOT] size=%d\n", BE32(hdr->totalsize)));
        D(kprintf("[BOOT] off_dt_struct=%d\n", BE32(hdr->off_dt_struct)));
        D(kprintf("[BOOT] off_dt_strings=%d\n", BE32(hdr->off_dt_strings)));
        D(kprintf("[BOOT] off_mem_rsvmap=%d\n", BE32(hdr->off_mem_rsvmap)));

        strings = (char*)dt + BE32(hdr->off_dt_strings);
        data = (uint32_t*)((char*)dt + BE32(hdr->off_dt_struct));

        if (hdr->off_mem_rsvmap)
        {
            struct fdt_reserve_entry *rsrvd = (void*)((intptr_t)dt + BE32(hdr->off_mem_rsvmap));

            while (rsrvd->address != 0 || rsrvd->size != 0) {
                D(kprintf("[BOOT]   reserved: %08x-%08x\n",
                    (uint32_t)BE64(rsrvd->address),
                    (uint32_t)(BE64(rsrvd->address) + BE64(rsrvd->size - 1)))
                );
                rsrvd++;
            }
        }

        do
        {
            token = BE32(*data++);

            switch (token)
            {
                case FDT_BEGIN_NODE:
                    root = dt_build_node(NULL);
                    break;
                case FDT_PROP:
                {
                    kprintf("[BOOT] Property outside root node?");
                    break;
                }
                default:
                    D(kprintf("[BOOT] unknown node %08x\n", token));
            }
        } while (token != FDT_END);
    }
    else
    {
        hdr = NULL;
    }

    return root;
}

static of_node_t * dt_find_by_phandle(uint32_t phandle, of_node_t *root)
{
    of_property_t *p = dt_find_property(root, "phandle");

    if (p && *((uint32_t *)p->op_value) == BE32(phandle))
        return root;
    else {
        of_node_t *c;
        for (c=root->on_children; c; c = c->on_next)
        {
            of_node_t *found = dt_find_by_phandle(phandle, c);
            if (found)
                return found;
        }
    }
    return NULL;
}

of_node_t * dt_find_node_by_phandle(uint32_t phandle)
{
    return dt_find_by_phandle(phandle, root);
}

#define MAX_KEY_SIZE    64
char ptrbuf[64];

of_node_t * dt_find_node(char *key)
{
    int i;
    of_node_t *node, *ret = NULL;

    if (*key == '/')
    {
        ret = root;

        while(*key)
        {
            key++;
            for (i=0; i < 63; i++)
            {
                if (*key == '/' || *key == 0)
                    break;
                ptrbuf[i] = *key;
                key++;
            }

            ptrbuf[i] = 0;

            for (node = ret->on_children; node; node = node->on_next)
            {
                if (!strcmp(node->on_name, ptrbuf))
                {
                    ret = node;
                    break;
                }
            }
        }
    }

    return ret;
}

of_property_t *dt_find_property(void *key, char *propname)
{
    of_node_t *node = (of_node_t *)key;
    of_property_t *p, *prop = NULL;

    if (node)
    {
        for (p=node->on_properties; p; p=p->op_next)
        {
            if (!strcmp(p->op_name, propname))
            {
                prop = p;
                break;
            }
        }
    }
    return prop;
}

char fill[] = "                         ";

void dt_dump_node(of_node_t *n, int level)
{
    of_property_t *p;
    of_node_t *c;

    kprintf("[BOOT] %s%s\n", &fill[25-2*level], n->on_name);
    for (p = n->on_properties; p; p = p->op_next)
    {
        kprintf("[BOOT] %s  %s=", &fill[25-2*level], p->op_name);
        for (unsigned i=0; i < p->op_length; i++) {
            char *pchar = (char*)(p->op_value) + i;
            if (*pchar >= ' ' && *pchar <= 'z')
                kprintf("%c", *pchar);
            else
                kprintf(".");
        }

        if (p->op_length) {
            kprintf(" (");
            unsigned max = 16;
            if (max > p->op_length)
                max = p->op_length;

            for (unsigned i=0; i < p->op_length; i++) {
                char *pchar = (char*)(p->op_value) + i;
                kprintf("%02x", *pchar);
            }
            kprintf(")");
        }
        kprintf("\n");
    }

    for (c = n->on_children; c; c = c->on_next)
    {
        dt_dump_node(c, level+1);
    }
}

void dt_dump_tree()
{
    kprintf("[BOOT] Device Tree dump:\n");

    dt_dump_node(root, 0);
}
