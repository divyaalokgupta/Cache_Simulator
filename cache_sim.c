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
        char name[10];
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

struct cache *create_cache(unsigned int blocksize, unsigned int assoc, unsigned int cachesize, char *name);

void delete_cache(struct cache *mem);

void print_simulator_config(struct cache *mem);

void print_cache(struct cache *mem);

void print_simulation_summary(struct cache *mem);

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
        L1 = create_cache(blocksize,L1_assoc,L1_size,"L1");
        if(L2_size >= 0 && L2_assoc >= 1)
        {
            struct cache *L2;
            L2 = create_cache(blocksize,L2_assoc,L2_size,"L2");
            L1->next = L2;
        }

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
        unsigned int instr = 0;
        print_simulator_config(L1);
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
                
                instr++;
                if(str[0] == 'r')
                {
                        printf("DEBUG::----------------------------------------\n");
                        printf("DEBUG::# %d: read %lx\n",instr,num_addr);
                        access_cache(L1,num_addr, str[0]);
                }
                else if(str[0] == 'w')
                {
                        printf("DEBUG::----------------------------------------\n");
                        printf("DEBUG::# %d: write %lx\n",instr,num_addr);
                        access_cache(L1,num_addr, str[0]);
                }
                else if(str[0] == ' ')
                {
                        printf("Null character in trace");
                }
        }

        printf("===== L1 contents =====\n");
        print_cache(L1);
        if(L2_size >= 0 && L2_assoc >= 1)
        {
            printf("\n===== L2 contents =====\n");
            print_cache(L1->next);
            delete_cache(L1->next);
        }
        print_simulation_summary(L1);
        delete_cache(L1);
        return 0;
}

struct cache *create_cache(unsigned int blocksize, unsigned int assoc, unsigned int cachesize, char *name)
{
        struct cache *mem = (struct cache*)malloc(sizeof(struct cache));
        strcpy(mem->name,name);
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

void print_simulator_config(struct cache *mem)
{
        printf("===== Simulator configuration =====\n");
        printf("  BLOCKSIZE:\t\t%d\n",mem->blocksize);
        printf("  L1_SIZE:\t\t%d\n",mem->tot_size);
        printf("  L1_ASSOC:\t\t%d\n",mem->assoc);
        printf("  VC_NUM_BLOCKS:\t%d\n",0);
        if(mem->next == NULL)
        {
            printf("  L2_SIZE:\t\t%d\n",0);
            printf("  L2_ASSOC:\t\t%d\n",0);
        }
        else
        {
            printf("  L2_SIZE:\t\t%d\n",mem->next->tot_size);
            printf("  L2_ASSOC:\t\t%d\n",mem->next->assoc);
        }
        printf("  trace_file:\t%s\n",mem->trace);
        printf("\n");
        return;
}

void print_cache(struct cache *mem)
{
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
            
            
            while(min < validblocks)
            {
                for (j=0;j<mem->assoc;j++)
                {
                    //if(mem->blocks[i*mem->assoc+j].count == min)
                    if(mem->blocks[i*mem->assoc+j].valid == 1 && mem->blocks[i*mem->assoc+j].count == min)
                    {
                        if (mem->blocks[i*mem->assoc+j].dirty == 0)
                            printf("%lx    ",mem->blocks[i*mem->assoc+j].tag);
                        else
                            printf("%lx D  ",mem->blocks[i*mem->assoc+j].tag);
                        ++min;

                    }
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

        return;
}

void print_simulation_summary(struct cache *mem)
{
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
        if(mem->next == NULL)
        {
            printf(" j. number of L2 reads:                           0\n");
            printf(" k. number of L2 read misses:                     0\n");
            printf(" l. number of L2 writes:                          0\n");
            printf(" m. number of L2 write misses:                    0\n");
            printf(" n. L2 miss rate:                            0.0000\n");
            printf(" o. number of writebacks from L2:                 0\n");
            printf(" p. total memory traffic:                     %d\n",mem->read_misses+mem->write_misses+mem->writebacks);
        }
        else
        {
            printf(" j. number of L2 reads:                           %d\n",mem->next->read_hits + mem->next->read_misses);
            printf(" k. number of L2 read misses:                     %d\n",mem->next->read_misses);
            printf(" l. number of L2 writes:                          %d\n",mem->next->write_hits + mem->next->write_misses);
            printf(" m. number of L2 write misses:                    %d\n",mem->next->write_misses);
            printf(" n. L2 miss rate:                            %0.4f\n",(float)(mem->next->read_misses+mem->next->write_misses)/(mem->next->read_hits+mem->next->read_misses+mem->next->write_hits+mem->next->write_misses));
            printf(" o. number of writebacks from L2:                 %d\n",mem->next->writebacks);
            printf(" p. total memory traffic:                     %d\n",mem->next->read_misses+mem->next->write_misses+mem->next->writebacks);
        }
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
                printf("%d,%d,%lx,%d\t",mem->blocks[(row*mem->assoc) + j].valid,mem->blocks[(row*mem->assoc) + j].dirty,mem->blocks[(row*mem->assoc) + j].tag, mem->blocks[(row*mem->assoc) + j].count);
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
             
     if(mode == 'r')
         printf("DEBUG:: %s read: %lx (tag %lx, index %lu)\n",mem->name,address,tag,index);
     else
         printf("DEBUG:: %s write: %lx (tag %lx, index %lu)\n",mem->name,address,tag,index);

     
     int column = check_cache_hit(mem,tag,index);
     if (column >= 0)
     {
         if ( mode == 'r')
         {
             printf("DEBUG:: %s read hit\n",mem->name);
             mem->read_hits++;
         }
         else
         {
             printf("DEBUG:: %s write hit\n",mem->name);
             mem->write_hits++;
         }
     }
     else
     {
         if ( mode == 'r')
         {
             printf("DEBUG:: %s read miss\n",mem->name);
             mem->read_misses++;
         }
         else
         {
             printf("DEBUG:: %s write miss\n",mem->name);
             mem->write_misses++;
         }

         column = create_space(mem,tag,index);
//         printf("%lu,Address: %lu\tColumn: %d Index: %d\n",mem->tot_size,address,column,index);
         assert(mem->blocks[index*mem->assoc + column].valid == 0);
         assert(mem->blocks[index*mem->assoc + column].dirty == 0);
         assert(mem->blocks[index*mem->assoc + column].tag == 0);
         assert(mem->blocks[index*mem->assoc + column].count == (mem->assoc - 1));
        
         if(mem->next != NULL)
         {
            /* Accessed main memory for data */
            access_cache(mem->next,address,'r');
         }
//         printf("%lu,Address: %lu\tColumn: %d Index: %d\n",mem->tot_size,address,column,index);

         mem->blocks[index*mem->assoc + column].valid = 1;
         mem->blocks[index*mem->assoc + column].tag = tag;
     }
     
     update_count(mem,index,column);
     printf("DEBUG:: %s update LRU\n",mem->name);
     
     if(mode == 'w')
     {
        mem->blocks[index*mem->assoc + column].dirty = 1;
        printf("DEBUG:: %s set dirty\n",mem->name);
     }

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
     unsigned int row = index * mem->assoc;
     int j;
     for(j=0;j<mem->assoc;j++)
     {
//        printf("DEBUG:: Valid: %d, Dirty: %d, Tag: %lx, Count: %d\n",mem->blocks[row + j].valid,mem->blocks[row + j].dirty,mem->blocks[row + j].tag,mem->blocks[row + j].count);
     }
     for(j=0;j<mem->assoc;j++)
     {
        assert(mem->blocks[row + j].count <= (mem->assoc - 1));
        if( mem->blocks[row + j].count == (mem->assoc - 1))
        {
             break;
        }
     }
     assert(j <= (mem->assoc - 1));

     unsigned long recreated_address;
     recreated_address = mem->blocks[row+j].tag<<mem->index_bits;
     recreated_address = recreated_address<<mem->block_offset_bits;
     recreated_address = recreated_address + (index<<mem->block_offset_bits);
    
     if(mem->blocks[row+j].valid == 1 && mem->blocks[row+j].dirty == 1)
     {
          mem->writebacks++;
          printf("DEBUG:: %s victim: %lx (tag %lx, index %lu, dirty)\n",mem->name,recreated_address,mem->blocks[row+j].tag,index);
          if(mem->next != NULL)
          {
              access_cache(mem->next,recreated_address,'w');
          }
     }
     else if (mem->blocks[row+j].valid == 1 && mem->blocks[row+j].dirty == 0)
     {
          //Silent Eviction
          printf("DEBUG:: %s victim: %lx (tag %lx, index %lu, clean)\n",mem->name,recreated_address,mem->blocks[row+j].tag,index);
     }
     else
     {
          printf("DEBUG:: %s victim: none\n",mem->name);
     }

     mem->blocks[row + j].tag = 0;
     mem->blocks[row + j].valid = 0;
     mem->blocks[row + j].dirty = 0;
     mem->blocks[row + j].count = mem->assoc - 1;

     return j;
}

