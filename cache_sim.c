#include<stdio.h>
#include<stdlib.h>
#include<assert.h>
#include<string.h>
#include<ctype.h>
#include<math.h>

#define MAX_ADDRESS_SIZE 32

struct cache_block {
        unsigned long tag;
        unsigned int valid;
        unsigned int dirty;
        unsigned int count;
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
        unsigned int rows;
        unsigned int read_hits;
        unsigned int read_misses;
        unsigned int write_hits;
        unsigned int write_misses;
        unsigned int writebacks;
        char trace[100];
        struct cache_block *blocks;
        struct cache *next;
};

struct cache *create_cache(unsigned int blocksize, unsigned int assoc, unsigned int cachesize);

void delete_cache(struct cache *mem);

void print_cache(struct cache *mem);

void access_cache(struct cache *mem, unsigned long address, char mode);

int check_cache_hit(struct cache *mem, unsigned long tag, unsigned long index);

int create_space(struct cache *mem, unsigned long tag, unsigned long index);

void update_count(struct cache *mem, unsigned int row, unsigned int col);

void display_row(struct cache *mem, unsigned int row);

unsigned long decode_index(struct cache *mem, unsigned long address);

unsigned long decode_tag(struct cache *mem, unsigned long address);

int main(int argc, char *argv[])
{
        if(argc <= 7)
        {
                printf("Usage: ./sim_cache <Block Size> <L1 Size> <L1 Associativity> <Victim Cache Number of Blocks> <L2 Size> <L2 Associativity> <Trace File>.\n");
                return 0;
        }

        unsigned int blocksize = atoi(argv[1]);
        unsigned int L1_size = atoi(argv[2]);
        unsigned int L1_assoc = atoi(argv[3]);
        unsigned int L1_vc_blocks = atoi(argv[4]);
        unsigned int L2_size = atoi(argv[5]);
        unsigned int L2_assoc = atoi(argv[6]);
        
        struct cache *L1 = (struct cache*)malloc(sizeof(struct cache));
        L1 = create_cache(blocksize,L1_assoc,L1_size);

        strcpy(L1->trace,argv[7]);
        FILE *trace = fopen(argv[7],"r");
        if(trace == NULL)
        {
                printf("Error: Could not open file.\n");
                return 0;
        }
        
        char str[12],address[12];
        unsigned long num_addr;
        int i;
        while(fgets(str, 12, trace) != NULL)
        {
                for(i=0;i<strlen(str);i++)
                    if(str[i] == '\n')
                      str[i] = '\0';

                for(i=2;i<=strlen(str);i++)
                {
                        address[i-2] = str[i];
                }
                address[i-2] = '\0';
                num_addr = (int)strtoul(address, NULL, 16);
                
                if(str[0] == 'r')
                {
                        access_cache(L1,num_addr, str[0]);
                }
                else if(str[0] == 'w')
                {
                        access_cache(L1,num_addr, str[0]);
                }
                else if(str[0] == ' ')
                {
                        printf("Null character in trace");
                }

        }

        print_cache(L1);
        delete_cache(L1);
        return 0;
}

struct cache *create_cache(unsigned int blocksize, unsigned int assoc, unsigned int cachesize)
{
        struct cache *mem = (struct cache*)malloc(sizeof(struct cache));
        mem->tot_size = cachesize;
        mem->assoc = assoc;
        mem->blocksize = blocksize;
        mem->block_offset_bits = log2(mem->blocksize);
        mem->rows = mem->tot_size / ( mem->assoc * mem->blocksize);
        mem->index_bits = log2(mem->rows);
        mem->tot_numblocks = mem->rows * mem->assoc;
        mem->tag_bits = MAX_ADDRESS_SIZE - (mem->index_bits + mem->block_offset_bits);
        mem->read_hits = 0;
        mem->read_misses = 0;
        mem->write_hits = 0;
        mem->write_misses = 0;
        mem->writebacks = 0;
        mem->next = NULL;
/*      printf("%d rows, %d way associative, %d bytes in each block, Index %d bits, Tag %d bits, Block Offset %d bits\n",mem->rows,mem->assoc, mem->blocksize, mem->index_bits,mem->tag_bits,mem->block_offset_bits); */
        
        mem->blocks = (struct cache_block*)malloc(sizeof(struct cache_block) * mem->rows * mem->assoc);
/*      printf("Allocated %d Bytes\n",sizeof(struct cache_block)*mem->rows*mem->assoc); */
        
/* Setting valid bits for all blocks to zero */
        unsigned int i;
        for(i=0;i<mem->tot_numblocks;i++)
        {
            mem->blocks[i].valid = 0;
            mem->blocks[i].count = mem->assoc-1;
            mem->blocks[i].dirty = 0;
            mem->blocks[i].tag = 0;
        }
        return mem;
}

void delete_cache(struct cache *mem)
{
        free(mem->blocks);
        free(mem);
}

void print_cache(struct cache *mem)
{
        printf("===== Simulator Configuration =====\n");
        printf("  BLOCKSIZE:\t\t%d\n",mem->blocksize);
        printf("  L1_SIZE:\t\t%d\n",mem->tot_size);
        printf("  L1_ASSOC:\t\t%d\n",mem->assoc);
        printf("  VC_NUM_BLOCKS:\t%d\n",0);
        printf("  L2_SIZE:\t\t%d\n",0);
        printf("  L2_ASSOC:\t\t%d\n",0);
        printf("  trace_file:\t%s\n",mem->trace);
        printf("\n");

        printf("===== L1 contents =====\n");
        unsigned int i,j;
        for(i=0;i<mem->rows;i++)
        {
            printf("  set   %d:   ",i);

            unsigned int validblocks = 0, min = 0;
            for (j=0;j<mem->assoc;j++)
            {
                if (mem->blocks[i*mem->assoc+j].valid == 1)
                {
                    validblocks++;
                }
            }

            for (j=0;j<mem->assoc;j++)
            {
                if(mem->blocks[i*mem->assoc+j].valid == 1 && mem->blocks[i*mem->assoc+j].count == min)
                {
                    if (mem->blocks[i*mem->assoc+j].dirty == 0)
                        printf("%lx    ",mem->blocks[i*mem->assoc+j].tag);
                    else
                        printf("%lx D  ",mem->blocks[i*mem->assoc+j].tag);
                    min++;

                    if(min < validblocks)
                        j = 0;
                    else
                        break;
                }
            }

//            for (j=0;j<mem->assoc;j++)
//            {   //Need to write code for sorting based on LRU
//                
//                if (mem->blocks[i*mem->assoc+j].valid == 1)
//                {
//                    if (mem->blocks[i*mem->assoc+j].dirty == 0)
//                        printf("%lx    ",mem->blocks[i*mem->assoc+j].tag);
//                    else
//                        printf("%lx D  ",mem->blocks[i*mem->assoc+j].tag);
//                }
//            }
            printf("\n");
        }
        
        printf("\n ===== Simulation results =====\n");
        printf(" a. number of L1 reads:                       %d\n",mem->read_hits+mem->read_misses);
        printf(" b. number of L1 read misses:                 %d\n",mem->read_misses);
        printf(" c. number of L1 writes:                      %d\n",mem->write_hits+mem->write_misses);
        printf(" d. number of L1 write misses:                %d\n",mem->write_misses);
        printf(" e. number of swap requests:                      0\n");
        printf(" f. swap request rate:                       0.0000\n");
        printf(" g. number of swaps:                              0\n");
        printf(" h. combined L1+VC miss rate:                %0.4f\n",(float)(mem->read_misses+mem->write_misses)/(mem->read_hits+mem->read_misses+mem->write_hits+mem->write_misses));
        printf(" i. number writebacks from L1/VC:                 %d\n",mem->writebacks);
        printf(" j. number of L2 reads:                           0\n");
        printf(" k. number of L2 read misses:                     0\n");
        printf(" l. number of L2 writes:                          0\n");
        printf(" m. number of L2 write misses:                    0\n");
        printf(" n. L2 miss rate:                            0.0000\n");
        printf(" o. number of writebacks from L2:                 0\n");
        printf(" p. total memory traffic:                     %d\n",mem->read_misses+mem->write_misses+mem->writebacks);
}

void update_count(struct cache *mem, unsigned int row, unsigned int col)
{
        unsigned int j;
        unsigned temp_count = mem->blocks[(row*mem->assoc) + col].count;
        for(j=0;j<mem->assoc;j++)
        {
            if(mem->blocks[(row*mem->assoc) + j].valid == 1 && mem->blocks[(row*mem->assoc) + j].count < temp_count)
                mem->blocks[(row*mem->assoc) + j].count++;
        }
        mem->blocks[(row*mem->assoc) + col].count = 0;
}

void display_row(struct cache *mem, unsigned int row )
{
        unsigned int j;
        for(j=0;j<mem->assoc;j++)
                printf("%d,%lu,%d\t",mem->blocks[(row*mem->assoc) + j].valid,mem->blocks[(row*mem->assoc) + j].tag, mem->blocks[(row*mem->assoc) + j].count);
        printf("\n");
}

unsigned long decode_index(struct cache *mem, unsigned long address)
{
      address = address>>mem->block_offset_bits;
      address = address<<(mem->block_offset_bits + mem->tag_bits);
      address = address>>(mem->block_offset_bits + mem->tag_bits);
      return address;
}

unsigned long decode_tag(struct cache *mem, unsigned long address)
{
      address = address>>mem->block_offset_bits;
      address = address>>mem->index_bits;
      return address;
}

void access_cache(struct cache *mem, unsigned long address, char mode)
{
     assert(mode == 'r' || mode == 'w');
     unsigned long index, tag;
     index = decode_index(mem,address);
     tag = decode_tag(mem,address);
     
     int column = check_cache_hit(mem,tag,index);
     if (column >= 0)
     {
         if ( mode == 'r')
         {
             mem->read_hits++;
         }
         else
         {
             mem->write_hits++;
         }
     }
     else
     {
         if ( mode == 'r')
         {
             mem->read_misses++;
         }
         else
         {
             mem->write_misses++;
         }

         column = create_space(mem,tag,index);
         assert(mem->blocks[index*mem->assoc + column].valid == 0);
         assert(mem->blocks[index*mem->assoc + column].dirty == 0);
         assert(mem->blocks[index*mem->assoc + column].tag == 0);
         assert(mem->blocks[index*mem->assoc + column].count == (mem->assoc - 1));

         mem->blocks[index*mem->assoc + column].valid = 1;
         mem->blocks[index*mem->assoc + column].tag = tag;
     }
     
     update_count(mem,index,column);
     
     if(mode == 'w')
        mem->blocks[index*mem->assoc + column].dirty = 1;

     return;
}

int check_cache_hit(struct cache *mem, unsigned long tag, unsigned long index)
{
     int j;
     unsigned int row = index * mem->assoc;

     for(j=0;j<mem->assoc;j++)
        if(mem->blocks[row + j].valid == 1 && mem->blocks[row + j].tag == tag)
             return j;
     return -1;
}

int create_space(struct cache *mem, unsigned long tag, unsigned long index)
{
     int j;
     unsigned int row = index * mem->assoc;
     for(j=0;j<mem->assoc;j++)
     {
        assert(mem->blocks[row + j].count <= (mem->assoc - 1));
        if( mem->blocks[row + j].count == (mem->assoc - 1) )
        {
             break;
        }
     }
     assert(j <= (mem->assoc - 1));
    
     if(mem->blocks[row+j].valid == 1 && mem->blocks[row+j].dirty == 1 )
     {
        //if valid call evict...essentially access upper memory
        mem->writebacks++;
     }

     mem->blocks[row + j].tag = 0;
     mem->blocks[row + j].valid = 0;
     mem->blocks[row + j].dirty = 0;
     mem->blocks[row + j].count = mem->assoc - 1;

     return j;
}
