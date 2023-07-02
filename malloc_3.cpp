#include <unistd.h>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <sys/mman.h>

#define MAX_ALLOCATE (1e8)
#define MAX_ORDER (10)
#define KB (1024)
#define MMAP_THRESHOLD (128*KB)
#define BLOCK_SIZE (128*KB)

struct MallocMetadata {
    size_t size;
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;
    int cookie; // challenge 4
    /**
     * \todo:
     * think about adding more fields
     * validate size of struct not bigger then 64 bytes(IMPORTENT!!!)
    */
};

// -------------------- Data structure global  -------------------- // 

// Initialization
static size_t meta_data_size = sizeof(MallocMetadata);
static size_t total_free_blocks = 0;
static size_t total_allocated_blocks = 0;
static size_t total_big_blocks = 0; // increase when using mmap()
static size_t total_blocks = total_free_blocks + total_allocated_blocks + total_big_blocks;
static size_t total_allocated_bytes = 0;
static bool first_smalloc = true;

//static size_t total_free_bytes_with_meta = 0;
//static size_t total_free_bytes = 0; // = total_free_bytes_with_meta - (total_free_blocks*meta_data_size)

MallocMetadata *bins[MAX_ORDER+1] = {nullptr};
//////////////////////////////////////////////

// Functions declerations
static void initialize_free_space();
static size_t total_free_bytes();
static size_t check_max_free_space();
static void* find_minimal_space(size_t size);
static void* split_blocks(int order,size_t size);
static void* find_buddy(MallocMetadata *block);
static MallocMetadata* remove_block_from_bin(int order);
static void insert_block_to_bin(MallocMetadata* place, int order);


/////////////////////////////////////////////

// Implementation part
size_t total_free_bytes(){

    size_t total_free_bytes_with_meta = 0;

    for (int order = 0; order < MAX_ORDER+1; order++){

        MallocMetadata *current = bins[order];
        int orderi_free_count = 0;

        while (!current) orderi_free_count ++, current = current->next;
        
        total_free_bytes_with_meta += orderi_free_count*128*pow(2,order);
    }

    return total_free_bytes_with_meta - (total_free_blocks*meta_data_size);
}
/////////////////////////////////////////////

static void* find_buddy(MallocMetadata *block){
    
    if(!block->is_free){
        return nullptr;
    }

    return (void*)((uintptr_t)block ^ block->size); 
}

static void initialize_free_space(){

    uintptr_t current_pb = (uintptr_t)sbrk(0);

    uintptr_t addr_for_block = (uintptr_t)sbrk((intptr_t)32*BLOCK_SIZE);

    uintptr_t addr_for_block = (uintptr_t)sbrk((intptr_t)32*BLOCK_SIZE);

    uintptr_t addr = addr_for_block + current_pb ;

    for(int i=0; i<32; i++){

        MallocMetadata *new_block = (MallocMetadata*)(intptr_t)addr + BLOCK_SIZE*i;
        if (i==31)
        {
            new_block->next = nullptr;
        }
        else{
            new_block->next = (MallocMetadata*)(intptr_t)addr + BLOCK_SIZE*(i+1);
        }

        if(i==0){
            new_block->prev = nullptr;
        }
        else{
            new_block->prev = (MallocMetadata*)(intptr_t)addr + BLOCK_SIZE*(i-1);
        }

        new_block->is_free = true;
        new_block->size = BLOCK_SIZE - meta_data_size;
        
        /**
         * \todo : new_block->cookie?
        */
        
    }

    total_free_blocks += 32;
    first_smalloc = false; 
}
static void* split_blocks(int order,size_t size){

    int min_order = MAX_ORDER;
    while(size <= ((128*pow(2,min_order))-meta_data_size) && min_order>=0){
        min_order -=1;
    }
    min_order++;

    int num_of_splits = order - min_order;

    if (num_of_splits == 0){
        // we don't need to split
        MallocMetadata* block_to_remove = remove_block_from_bin(min_order);
        block_to_remove->is_free = false;
        total_free_blocks--;
        total_allocated_blocks++;
        total_allocated_bytes+=size;
        //block_to_remove->cookie?

        return (void*)block_to_remove;
    }
    else{
        // need to split
        for (int i = order; i > min_order; i--){
            void * addr_to_split = (void*)remove_block_from_bin(order);
            MallocMetadata* buddy1 =  (MallocMetadata*)addr_to_split;
            uintptr_t buddy2_start_addr =  (uintptr_t)buddy1 + 128*pow(2,order-1);
            MallocMetadata* buddy2 = (MallocMetadata*)buddy2_start_addr;
            insert_block_to_bin(buddy1, order-1);
            insert_block_to_bin(buddy2, order-1);
            total_free_blocks++; // we remove one but added two buddies
        }

        // finally remove the right free space and malloc
        MallocMetadata* block_to_remove = remove_block_from_bin(min_order);
        block_to_remove->is_free = false;
        total_free_blocks--;
        total_allocated_blocks++;
        total_allocated_bytes+=size;
        //block_to_remove->cookie?

        return (void*)block_to_remove;
        
    }
    
}

static void* find_minimal_space(size_t size){

    for (int order=0; order<MAX_ORDER+1; order++){
        if(bins[order]){
            //case there is free blocks in order i
            if((128*pow(2,order)-meta_data_size) >= size){
                // case size order good enough
                return split_blocks(order,size);
            }
        }
    }

    return (void*)-1; // for bugging erase after
}

MallocMetadata* remove_block_from_bin(int order){
    if(!bins[order]){
        //no blocks
        std::printf("somthing wrong in remove block from bin");
        //return nullptr;
    }
    if(!bins[order]->next){
        //one block
        MallocMetadata* tmp = bins[order];
        bins[order]=nullptr;
        return tmp;
    }
    //more then one
    MallocMetadata* tmp = bins[order];
    bins[order] =  bins[order]->next;
    bins[order]->prev = nullptr;
    return tmp;
}

void insert_block_to_bin(MallocMetadata* place, int order){
    MallocMetadata* runner = bins[order];
    if (!runner)
    {
        bins[order]= place;
        bins[order]->prev = nullptr;
        bins[order]->next = nullptr;
        return;
    }
    
    MallocMetadata* runner_prev;
    while (runner)
    {
        runner_prev = runner->prev;
        if((uintptr_t)place < (uintptr_t)runner)
        {
            //MallocMetadata* tmp = runner->prev;
            runner_prev->next = place;
            runner->prev = place;
            place->next = runner;
            place->prev = runner_prev;
            return;
        }

        runner= runner->next;

    }

    //insert to tale
    runner_prev->next = place;
    place->prev = runner_prev;
    place->next = nullptr;
}

size_t check_max_free_space(){
    for (int i = MAX_ORDER; i >= 0 ; i--)
    {
        if(bins[i]){
            return 128*pow(2, i)-meta_data_size;
        }
    }
    return 0 ;
}

// -------------------- Better Malloc Implementation  -------------------- // 

void* smalloc(size_t size){

    if(size==0 || size>MAX_ALLOCATE)
    {
        return nullptr;
    }

    if(first_smalloc){
        initialize_free_space();
    }

    if(size < MMAP_THRESHOLD){
        if(check_max_free_space() >= size){
            void* place = find_minimal_space(size);
            return place+meta_data_size;
        }
        else {
            // there is not enough free space that can holds size
            // need to check
            return nullptr;

        }
    }

    else{ 
        // size >= MMAP_THRESHOLD
        void *big_block_addr = mmap(NULL, (size + meta_data_size), PROT_WRITE | PROT_READ, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        if (big_block_addr == (void *)-1)
        {
            return nullptr;
        }
        MallocMetadata* big_block = (MallocMetadata *)big_block_addr;
        big_block->is_free = false;
        big_block->next = nullptr;
        big_block->prev = nullptr; 
        //big_block->cookie = ?;

        total_allocated_blocks++;
        total_allocated_bytes += size ; 
        //check about that

        return big_block_addr+meta_data_size;
        
    }
}


void *scalloc(size_t num, size_t size) {


}

void sfree(void *p) {
    if (!p){
        return;
    }

    MallocMetadata* block_let_it_go = (MallocMetadata*)((uintptr_t)p - (uintptr_t)meta_data_size);
    if( block_let_it_go->is_free ){
        return;
    }

    if (block_let_it_go->size >= MMAP_THRESHOLD)
    {
        //big block munmmp
    }
    else
    {
        /*

        insert( block let it go);
        block_let_it_go->is_free = true;

        while ( buddy)
        {
            merged block = merge (let it go , buddies );
            new_remove( merged block, order );
            insert(merged block , order+1);
            buddy = merged block;
        }
        
        
        
        
        
        */


        block_let_it_go->is_free = true;


    }
    
    
}


void *srealloc(void *oldp, size_t size) {

    
}



size_t _num_free_blocks()
{
    return total_free_blocks;
}

size_t _num_free_bytes()
{
    return total_free_bytes();
}

size_t _num_allocated_blocks()
{
    return total_allocated_blocks;
}

size_t _num_allocated_bytes()
{
    return total_allocated_bytes;
}

size_t _num_meta_data_bytes()
{
    return _size_meta_data() * _num_allocated_blocks();
}

size_t _size_meta_data()
{
    return meta_data_size;
}