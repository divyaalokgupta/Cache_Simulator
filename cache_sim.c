#include<stdio.h>
#include<stdlib.h>
#include<assert.h>
#include<string.h>
#include<ctype.h>
#include<math.h>

#define MAX_ADDRESS_SIZE 32

int DEBUG = 0;

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
        unsigned int swap_requests;
        unsigned int swaps;
        char trace[100];
        struct cache_block *blocks;
        struct cache *next;
        struct cache *vc;
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

int access_victim_cache(struct cache *mem, unsigned long tag, unsigned long index, char mode);

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
        struct cache *L2 = NULL;
        struct cache *vc = NULL;
        L1 = create_cache(blocksize,L1_assoc,L1_size,"L1");
        
        if(L1_vc_blocks > 0)
        {
             vc = (struct cache*)malloc(sizeof(struct cache));
             vc = create_cache(blocksize,L1_vc_blocks,L1_vc_blocks*blocksize,"VC");
             L1->vc = vc;
        }

        if(L2_size > 0 && L2_assoc >= 1)
        {
            L2 = (struct cache*)malloc(sizeof(struct cache));
            L2 = create_cache(blocksize,L2_assoc,L2_size,"L2");
            L1->next = L2;
        }

        if(L1_vc_blocks > 0 && L2_size > 0 && L2_assoc >= 1)
        {
            vc->next = L2;
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
                        if (DEBUG == 1 ) {printf("----------------------------------------\n");}
                        if (DEBUG == 1 ) {printf("# %d: read %lx\n",instr,num_addr);}
                        access_cache(L1,num_addr, str[0]);
                }
                else if(str[0] == 'w')
                {
                        if (DEBUG == 1 ) {printf("----------------------------------------\n");}
                        if (DEBUG == 1 ) {printf("# %d: write %lx\n",instr,num_addr);}
                        access_cache(L1,num_addr, str[0]);
                }
                else if(str[0] == ' ')
                {
                        printf("Null character in trace");
                }
                //print_cache(L1);
                //print_cache(L1->vc);
        }
        fclose(trace);

        printf("===== L1 contents =====\n");
        print_cache(L1);
        
        if(L1_vc_blocks > 0)
        {
            printf("===== VC contents =====\n");
            print_cache(vc);
        }
    
        if(L2_size >= 0 && L2_assoc >= 1)
        {
            printf("\n===== L2 contents =====\n");
            print_cache(L1->next);
        }
        print_simulation_summary(L1);

        if(L2_size > 0 && L2_assoc >= 1)
        {
            delete_cache(L2);
        }
        else
        {
            free(L2);
        }
        
        if(L1_vc_blocks > 0)
        {
            delete_cache(vc);
        }
        else
        {
            free(vc);
        }
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
        mem->vc = NULL;
        mem->swaps = 0;
        mem->swap_requests = 0;
        
        mem->blocks = (struct cache_block*)malloc(sizeof(struct cache_block) * mem->rows * mem->assoc);
        
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
//                  if(mem->blocks[i*mem->assoc+j].count == min)
                    if(mem->blocks[i*mem->assoc+j].valid == 1 && mem->blocks[i*mem->assoc+j].count == min)
                    {
                        if (mem->blocks[i*mem->assoc+j].dirty == 0)
                            printf("%lx\t    ",mem->blocks[i*mem->assoc+j].tag);
                            //printf("%lx,%d,%d\t    ",mem->blocks[i*mem->assoc+j].tag,mem->blocks[i*mem->assoc+j].valid,mem->blocks[i*mem->assoc+j].count);
                        else
                            printf("%lx D\t  ",mem->blocks[i*mem->assoc+j].tag);
                            //printf("%lx,%d,%d D\t  ",mem->blocks[i*mem->assoc+j].tag,mem->blocks[i*mem->assoc+j].valid,mem->blocks[i*mem->assoc+j].count);
                        ++min;
                    }
                }
            }
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

        if(mem->vc == NULL)
        {
            printf(" e. number of swap requests:                      0\n");
            printf(" f. swap request rate:                       0.0000\n");
            printf(" g. number of swaps:                              0\n");
            printf(" h. combined L1+VC miss rate:                %0.4f\n",(float)(mem->read_misses+mem->write_misses)/(mem->read_hits+mem->read_misses+mem->write_hits+mem->write_misses));
            printf(" i. number writebacks from L1/VC:                 %d\n",mem->writebacks);
        }
        else
        {
            printf(" e. number of swap requests:                      %d\n",mem->swap_requests);
            printf(" f. swap request rate:                       %0.4f\n",(float)mem->swap_requests/(mem->read_hits+mem->read_misses+mem->write_hits+mem->write_misses));
            printf(" g. number of swaps:                              %d\n",mem->swaps);
            printf(" h. combined L1+VC miss rate:                %0.4f\n",(float)(mem->read_misses+mem->write_misses - mem->vc->read_hits - mem->vc->write_hits + mem->vc->read_misses + mem->vc->write_misses)/(mem->read_hits+mem->read_misses+mem->write_hits+mem->write_misses));
            printf(" i. number writebacks from L1/VC:                 %d\n",mem->writebacks+mem->vc->writebacks);
        }

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
            printf(" n. L2 miss rate:                            %0.4f\n",(float)(mem->next->read_misses)/(mem->next->read_misses+mem->next->read_hits));
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
      if(mem->index_bits == 0)
            return 0;
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
     int victim_cache_help = 0;
     
     if(mode == 'r')
     {
        if (DEBUG == 1 ) {printf(" %s read: %lx (tag %lx, index %lu)\n",mem->name,((address>>mem->block_offset_bits)<<mem->block_offset_bits),tag,index);}
     }
     else if(mode == 'w') 
     {
        if (DEBUG == 1 ) {printf(" %s write: %lx (tag %lx, index %lu)\n",mem->name,((address>>mem->block_offset_bits)<<mem->block_offset_bits),tag,index);}
     }

     int column = check_cache_hit(mem,tag,index);
     if (column >= 0)
     {
         if ( mode == 'r')
         {
             if (DEBUG == 1 ) {printf(" %s hit\n",mem->name);}
             mem->read_hits++;
         }
         else
         {
             if (DEBUG == 1 ) {printf(" %s hit\n",mem->name);}
             mem->write_hits++;
         }
     }
     else
     {
         if ( mode == 'r')
         {
             if (DEBUG == 1 ) {printf(" %s miss\n",mem->name);}
             mem->read_misses++;
         }
         else
         {
             if (DEBUG == 1 ) {printf(" %s miss\n",mem->name);}
             mem->write_misses++;
         }

         if(mem->vc != NULL)
         {
             victim_cache_help = access_victim_cache(mem,tag,index,mode);
         }

         if(victim_cache_help == 0)
         {
            column = create_space(mem,tag,index);
            assert(mem->blocks[index*mem->assoc + column].valid == 0);
            assert(mem->blocks[index*mem->assoc + column].dirty == 0);
            assert(mem->blocks[index*mem->assoc + column].tag == 0);
            assert(mem->blocks[index*mem->assoc + column].count == (mem->assoc - 1));
         }
        
         if(mem->next != NULL)
         {
            access_cache(mem->next,address,'r');
         }
         if(victim_cache_help == 0)
         {
             //INFO: Get data from upper level memory
             mem->blocks[index*mem->assoc + column].valid = 1;
             mem->blocks[index*mem->assoc + column].tag = tag;
         }
     }

     if(victim_cache_help == 0)
     {
        update_count(mem,index,column);
        if (DEBUG == 1 ) {printf(" %s update LRU\n",mem->name);}
     }

     
     if(mode == 'w')
     {
        if(victim_cache_help == 0)
        {
            mem->blocks[index*mem->assoc + column].dirty = 1;
            if (DEBUG == 1 ) {printf(" %s set dirty\n",mem->name);}
        }
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
         if (DEBUG == 1 )
         {
             //printf(" Valid: %d, Dirty: %d, Tag: %lx, Count: %d\n",mem->blocks[row + j].valid,mem->blocks[row + j].dirty,mem->blocks[row + j].tag,mem->blocks[row + j].count);
         }
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

     unsigned long identified_block_recreated_address;
     identified_block_recreated_address = mem->blocks[row+j].tag<<mem->index_bits;
     identified_block_recreated_address = identified_block_recreated_address<<mem->block_offset_bits;
     identified_block_recreated_address = identified_block_recreated_address + (index<<mem->block_offset_bits);
    
     if(mem->blocks[row+j].valid == 1 && mem->blocks[row+j].dirty == 1)
     {
          if (DEBUG == 1)
          {
              printf(" %s victim: %lx (tag %lx, index %lu, dirty)\n",mem->name,identified_block_recreated_address,mem->blocks[row+j].tag,index);
          }

          if(mem->next != NULL)
          {
              access_cache(mem->next,identified_block_recreated_address,'w');
          }
          //INFO: else write back to main memory
          mem->writebacks++;
     }
     else if (mem->blocks[row+j].valid == 1 && mem->blocks[row+j].dirty == 0)
     {
          if (DEBUG == 1 )
          {
              printf(" %s victim: %lx (tag %lx, index %lu, clean)\n",mem->name,identified_block_recreated_address,mem->blocks[row+j].tag,index);
          }
          //Silent Eviction
     }
     else
     {
        if (DEBUG == 1 ) { printf(" %s victim: none\n",mem->name);}
     }

     mem->blocks[row + j].tag = 0;
     mem->blocks[row + j].valid = 0;
     mem->blocks[row + j].dirty = 0;
     mem->blocks[row + j].count = mem->assoc - 1;

     return j;
}

int access_victim_cache(struct cache *mem, unsigned long tag, unsigned long index, char mode)
{
     int j;
     unsigned int row = index * mem->assoc;
     for(j=0;j<mem->assoc;j++)
     {
        assert(mem->blocks[row + j].count <= (mem->assoc - 1));
        if( mem->blocks[row + j].valid == 0)
        {
             return 0;
        }
     }
     unsigned long required_address,vc_tag;
     required_address = tag<<mem->index_bits;
     required_address = required_address<<mem->block_offset_bits;
     required_address = required_address + (index<<mem->block_offset_bits);

     for(j=0;j<mem->assoc;j++)
     {
        assert(mem->blocks[row + j].count <= (mem->assoc - 1));
        if( mem->blocks[row + j].count == (mem->assoc - 1))
        {
             break;
        }
     }
     assert(j <= (mem->assoc - 1));

     unsigned long identified_block_recreated_address;
     identified_block_recreated_address = mem->blocks[row+j].tag<<mem->index_bits;
     identified_block_recreated_address = identified_block_recreated_address<<mem->block_offset_bits;
     identified_block_recreated_address = identified_block_recreated_address + (index<<mem->block_offset_bits);
     
     if (DEBUG == 1) {
         if (mem->blocks[row+j].dirty == 1)
             printf(" %s victim: %lx (tag %lx, index %lu, dirty)\n",mem->name,identified_block_recreated_address,mem->blocks[row+j].tag,index);
         else
             printf(" %s victim: %lx (tag %lx, index %lu, clean)\n",mem->name,identified_block_recreated_address,mem->blocks[row+j].tag,index);
     }
     
     if (DEBUG == 1) {printf(" %s swap req: [%lx, %lx]\n",mem->vc->name,required_address,identified_block_recreated_address);}
     mem->swap_requests++;

     assert(mem->vc->rows == 1);

     //victim hit or miss
     int column;
     column = check_cache_hit(mem->vc,decode_tag(mem->vc,required_address),0);
     if(column < 0)
     {
         if (mode == 'r')
         {
             mem->vc->read_misses++;
         }
         else if (mode == 'w')
         {
             mem->vc->write_misses++;
         }
         if (DEBUG == 1) {printf(" %s miss \n",mem->vc->name);}
         column = create_space(mem->vc,decode_tag(mem->vc,identified_block_recreated_address),0);
         mem->vc->blocks[column].tag = decode_tag(mem->vc,identified_block_recreated_address);
         mem->vc->blocks[column].dirty = mem->blocks[row+j].dirty;
         mem->vc->blocks[column].valid = 1;
         update_count(mem->vc,0,column);
         if (DEBUG == 1 ) {printf(" %s update LRU\n",mem->vc->name);}
         mem->blocks[row + j].valid = 1;
         mem->blocks[row + j].tag = tag;
         update_count(mem,index,j);
         if (DEBUG == 1 ) {printf(" %s update LRU\n",mem->name);}
         if(mode == 'r')
         {
             mem->blocks[row + j].dirty = 0;
         }
         else if (mode == 'w')
         {
             mem->blocks[row + j].dirty = 1;
             if (DEBUG == 1 ) {printf(" %s set dirty\n",mem->name);}
         }
     }
     else
     {
         if (mode == 'r')
         {
             mem->vc->read_hits++;
         }
         else if (mode == 'w')
         {
             mem->vc->write_hits++;
         }
         mem->swaps++;
         if (DEBUG == 1) {
            if (mem->vc->blocks[column].dirty == 1)
                printf(" %s hit: %lx (dirty)\n",mem->vc->name,required_address);
            else
                 printf(" %s hit: %lx (clean)\n",mem->vc->name,required_address);
         }
         mem->vc->blocks[column].tag = decode_tag(mem->vc,identified_block_recreated_address);
         int temp = mem->vc->blocks[column].dirty;
         mem->vc->blocks[column].dirty = mem->blocks[row+j].dirty;
         mem->vc->blocks[column].valid = 1;
         update_count(mem->vc,0,column);
         if (DEBUG == 1 ) {printf(" %s update LRU\n",mem->vc->name);}
         mem->blocks[row + j].valid = 1;
         mem->blocks[row + j].tag = tag;
         update_count(mem,index,j);
         if (DEBUG == 1 ) {printf(" %s update LRU\n",mem->name);}
         if(mode == 'r')
         {
             mem->blocks[row + j].dirty = temp;
         }
         else if (mode == 'w')
         {
             mem->blocks[row + j].dirty = 1;
             if (DEBUG == 1 ) {printf(" %s set dirty\n",mem->name);}
         }
     }
         printf("%d %d %d %d\n",mem->vc->read_hits,mem->vc->read_misses,mem->vc->write_hits,mem->vc->write_misses);
     return 1;
}
