#include<stdio.h>
#include<stdlib.h>
#include<assert.h>
#include<string.h>
#include<ctype.h>
#include<math.h>

#define MAX_ADDRESS_SIZE 32

struct cache_block{
        char tag[MAX_ADDRESS_SIZE];
        unsigned int valid;
        unsigned int dirty;
        unsigned int counter;
};

struct cache {
        unsigned int blocksize;
        unsigned int tot_numblocks;
        unsigned int tot_size;
        unsigned int assoc;
        unsigned int tot_numsets;
        unsigned int tag_bits;
        unsigned int index_bits;
        unsigned int block_offset_bits;
        unsigned int sets;
        unsigned int read_hits;
        unsigned int read_misses;
        unsigned int write_hits;
        unsigned int write_misses;
        struct cache_block *blocks;
        struct cache *next;
};

struct cache *create_cache(unsigned int blocksize, unsigned int assoc, unsigned int cachesize);

void delete_cache(struct cache *mem);

void print_cache(struct cache *mem);

void read_from_cache(struct cache *mem, unsigned long address);

void write_to_cache(struct cache *mem, unsigned long address);

void update_counter(struct cache *mem, unsigned int row);

unsigned long fetch(struct cache*mem, unsigned long address);

int main(int argc, char *argv[])
{
        if(argc <= 7)
        {
                printf("Usage: ./sim_cache <Block Size> <L1 Size> <L1 Associativity> <Victim Cache Number of Blocks> <L2 Size> <L2 Associativity> <Trace File>.\n");
                return 0;
        }

        printf("%s\n",argv[1]);
        printf("%s\n",argv[2]);
        printf("%s\n",argv[3]);
        printf("%s\n",argv[4]);
        printf("%s\n",argv[5]);
        printf("%s\n",argv[6]);
        printf("%s\n",argv[7]);

        unsigned int blocksize = atoi(argv[1]);
        unsigned int L1_size = atoi(argv[2]);
        unsigned int L1_assoc = atoi(argv[3]);
        unsigned int L1_vc_blocks = atoi(argv[4]);
        unsigned int L2_size = atoi(argv[5]);
        unsigned int L2_assoc = atoi(argv[6]);

        struct cache *L1 = (struct cache*)malloc(sizeof(struct cache));
        create_cache(blocksize,L1_assoc,L1_size);
        delete_cache(L1);
        return 0;
    
}


struct cache *create_cache(unsigned int blocksize, unsigned int assoc, unsigned int cachesize)
{
        struct cache *L = (struct cache*)malloc(sizeof(struct cache));
        L->tot_size = cachesize;
        L->assoc = assoc;
        L->blocksize = blocksize;
        L->sets = L->tot_size / ( L->assoc * L->blocksize);
        L->index_bits = log2(L->tot_numsets);
        L->block_offset_bits = log2(L->blocksize);
        L->tag_bits = MAX_ADDRESS_SIZE - (L->index_bits + L->block_offset_bits);
        printf("Index %d bits, Tag %d bits, Block Offset %d bits\n",L->index_bits,L->tag_bits,L->block_offset_bits);
        L->read_hits = 0;
        L->read_misses = 0;
        L->write_hits = 0;
        L->write_misses = 0;
        L->sets = 0;
        L->next = NULL;
        L->blocks = (struct cache_block*)malloc(sizeof(struct cache_block));
        return L;
}

void delete_cache(struct cache *mem)
{
        free(mem);     
}
