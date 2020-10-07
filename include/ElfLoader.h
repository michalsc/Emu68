#ifndef _ELFLOADER_H
#define _ELFLOADER_H

void * LoadELFFile(void *mem, void *load_address);
int GetElfSize(void *file, uint32_t *size_rw, uint32_t *size_ro);

#endif /* _ELFLOADER_H */
