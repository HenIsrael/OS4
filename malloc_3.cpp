#include <unistd.h>
#include <cstring>
#include <cstdint>
#include <sys/mman.h>
#include <cmath>
#include <cstdio>
#include <iostream>


#define MAX_ALLOCATE (1e8)
#define MAX_ORDER (10)
#define KB (1024)
#define MIN_BLOCK_SIZE (128)
#define MAX_BLOCK_SIZE (128*KB)
#define INITIAL_BLOCKS_NUM (32)
#define FREE_SPACE_CHUNK (MAX_BLOCK_SIZE*INITIAL_BLOCKS_NUM)

/**
 * \todo:
 * deal with challenge 4
 * finish writing sfree
 * check when to update global fields
 * write realloc
*/

struct MallocMetadata {
    int sweet_cookie;
    size_t size;
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;
};

// -------------------- Data structure global  -------------------- // 

// Initialization
static int cookie_recipe = std::rand();
static size_t meta_data_size = sizeof(MallocMetadata);
static size_t total_free_blocks = 0;
static size_t total_allocated_blocks = 0;
static size_t total_meta_data_bytes = 0;
static size_t total_allocated_bytes = 0;
static bool first_smalloc = true;

MallocMetadata *bins[MAX_ORDER+1] = {nullptr};
//////////////////////////////////////////////

// Functions declerations
static void* first_alignment_blocks();
static void* initialize_free_space();
static size_t total_free_bytes();
static size_t check_max_free_space();
static void* find_minimal_space(size_t size);
static void* split_blocks(int order,size_t size);
static MallocMetadata* merge_buddies(MallocMetadata *buddy1, MallocMetadata *buddy2);
static MallocMetadata* find_buddy(MallocMetadata *block);
static MallocMetadata* remove_block_from_bin(int order);
static void remove_block_from_bin(MallocMetadata* block, int order);
static void insert_block_to_bin(MallocMetadata* place, int order);
static bool malicious_attack(MallocMetadata* block);
//////////////////////////////////////////////////////////////////

// Implementation part
static size_t total_free_bytes(){

    size_t total_free_bytes_with_meta = 0;
    for (int order = 0; order < MAX_ORDER+1; order++){

        MallocMetadata *current = bins[order];
        if(malicious_attack(current)){
            exit(0xdeadbeef);
        }

        int orderi_free_count = 0;

        while (!current) orderi_free_count ++, current = current->next;
        
        total_free_bytes_with_meta += orderi_free_count*MIN_BLOCK_SIZE*pow(2,order);
    }

    return total_free_bytes_with_meta - (total_free_blocks*meta_data_size);
}
static void* first_alignment_blocks(){

    void *curr_prog_break = sbrk(0);
    if(curr_prog_break == (void*)(-1)){
        return nullptr;
    }

    intptr_t allign = FREE_SPACE_CHUNK + MAX_BLOCK_SIZE - (intptr_t)curr_prog_break%MAX_BLOCK_SIZE;
    void *addr_after_aligment = sbrk(allign);
    if(addr_after_aligment == (void*)(-1)){
        return nullptr;
    }

    return addr_after_aligment;
}

static void* initialize_free_space(){

    if (!first_alignment_blocks()){
        return nullptr;
    }
    
    void *addr = sbrk((intptr_t)FREE_SPACE_CHUNK);
    if(addr == (void*)(-1)){
        return nullptr;
    }

    for(int i=0; i<INITIAL_BLOCKS_NUM; i++){

        MallocMetadata *new_block = (MallocMetadata*)(intptr_t)addr + MAX_BLOCK_SIZE*i;

        if (i == INITIAL_BLOCKS_NUM -1){ // special case for last block
            new_block->next = nullptr;
        }

        else{
            new_block->next = (MallocMetadata*)(intptr_t)addr + MAX_BLOCK_SIZE*(i+1);
        }

        if(i==0){ // special case for first clock
            new_block->prev = nullptr;
        }
        else{
            new_block->prev = (MallocMetadata*)(intptr_t)addr + MAX_BLOCK_SIZE*(i-1);
        }

        new_block->is_free = true;
        new_block->size = MAX_BLOCK_SIZE - meta_data_size;
        new_block->sweet_cookie = cookie_recipe;
    }


    total_free_blocks += INITIAL_BLOCKS_NUM;
    total_allocated_blocks += INITIAL_BLOCKS_NUM;
    total_allocated_bytes += FREE_SPACE_CHUNK;
    total_meta_data_bytes += INITIAL_BLOCKS_NUM*meta_data_size;

    first_smalloc = false; 
    return addr;
}

static void* split_blocks(int order,size_t size){

    int min_order = MAX_ORDER;
    while(size <= ((MIN_BLOCK_SIZE*pow(2,min_order))-meta_data_size) && min_order>=0){
        min_order -=1;
    }
    min_order++;

    int num_of_splits = order - min_order;

    if (num_of_splits == 0){
        // we don't need to split
        MallocMetadata* block_to_remove = remove_block_from_bin(min_order);
        block_to_remove->is_free = false;

        total_free_blocks--;
        //block_to_remove->cookie?

        return (void*)block_to_remove;
    }
    else{
        // need to split
        for (int i = order; i > min_order; i--){
            void * addr_to_split = (void*)remove_block_from_bin(order);
            MallocMetadata* buddy1 =  (MallocMetadata*)addr_to_split;

            if(malicious_attack(buddy1)){
                exit(0xdeadbeef);
            }

            uintptr_t buddy2_start_addr =  (uintptr_t)buddy1 + MIN_BLOCK_SIZE*pow(2,order-1);
            MallocMetadata* buddy2 = (MallocMetadata*)buddy2_start_addr;

            if(malicious_attack(buddy2)){
                exit(0xdeadbeef);
            }

            buddy1->size = MIN_BLOCK_SIZE*pow(2,order-1) - meta_data_size;
            buddy2->size = MIN_BLOCK_SIZE*pow(2,order-1) - meta_data_size;

            insert_block_to_bin(buddy1, order-1);
            insert_block_to_bin(buddy2, order-1);

            total_meta_data_bytes += meta_data_size;
            total_free_blocks++; // we remove one but added two buddies
            total_allocated_blocks++;

            buddy1->is_free = true;
            buddy1->sweet_cookie = cookie_recipe;

            buddy2->is_free = true;
            buddy2->sweet_cookie =cookie_recipe;

        }

        // finally remove the right free space and malloc
        MallocMetadata* block_to_remove = remove_block_from_bin(min_order);
        block_to_remove->is_free = false;

        total_free_blocks--;

        return (void*)block_to_remove;
        
    }
    
}

static MallocMetadata* find_buddy(MallocMetadata *block){
    
    if(!block->is_free){
        return nullptr;
    }

    void *potential_buddy_addr = (void*)((uintptr_t)block ^ block->size);

    // check existence for buddy
    for(int order=0; order<MAX_ORDER+1; order++){

        MallocMetadata *current = bins[order];

        if(malicious_attack(current)){
            exit(0xdeadbeef);
        } 

        while (current)
        {
            if(current == potential_buddy_addr){
                return current; // yes buddy :)
            }
            current = current->next;
        }
    
    }

    return nullptr; // no buddy :(
}

static MallocMetadata * merge_buddies(MallocMetadata *buddy1, MallocMetadata *buddy2){

    MallocMetadata *big_monster = (MallocMetadata*) (std::min((uintptr_t)buddy1, (uintptr_t)buddy2));

    if(malicious_attack(big_monster)){
        exit(0xdeadbeef);
    }
    big_monster->size = buddy1->size + buddy2->size;
    big_monster->sweet_cookie = cookie_recipe;
    big_monster->next = nullptr;
    big_monster->prev = nullptr;

    return big_monster;
}

static void* find_minimal_space(size_t size){

    for (int order=0; order<MAX_ORDER+1; order++){
        if(bins[order]){
            //case there is free blocks in order i
            if((MIN_BLOCK_SIZE*pow(2,order)-meta_data_size) >= size){
                // case size order good enough
                return split_blocks(order,size);
            }
        }
    }

    return (void*)-1; // for bugging erase after
}

static MallocMetadata* remove_block_from_bin(int order){
    if(!bins[order]){
        //no blocks
        std::printf("somthing wrong in remove block from bin");
        //return nullptr;
    }
    if(!bins[order]->next){
        //one block
        MallocMetadata* tmp = bins[order];

        if(malicious_attack(tmp)){
            exit(0xdeadbeef);
        }

        bins[order]=nullptr;
        return tmp;
    }
    //more then one
    MallocMetadata* tmp = bins[order];

    if(malicious_attack(tmp)){
        exit(0xdeadbeef);
    }

    bins[order] =  bins[order]->next;
    bins[order]->prev = nullptr;
    return tmp;
}

static void remove_block_from_bin(MallocMetadata* block, int order){

    MallocMetadata *curr = bins[order];

    if(malicious_attack(curr)){
        exit(0xdeadbeef);
    }
    while(!curr->next){
        // block to remove in the middle
        if(curr = block){
            block->prev->next = block->next;
            block->next->prev = block->prev;
            block->next = nullptr;
            block->prev = nullptr;
            return;
        }

        curr = curr->next; 
    }
         
    if (curr = bins[order]){
        //the block to remove is head
        remove_block_from_bin(order);
        return;
    }
    else{
        // the block to remove is tail
        block->prev->next = nullptr;
        block->prev = nullptr;
        block->next = nullptr;
        return;
    }

return;

}

static void insert_block_to_bin(MallocMetadata* place, int order){
    MallocMetadata* runner = bins[order];

    if(malicious_attack(runner)){
        exit(0xdeadbeef);
    }
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

        if(malicious_attack(runner_prev)){
            exit(0xdeadbeef);
        }

        if((uintptr_t)place < (uintptr_t)runner)
        {
            runner_prev->next = place;
            runner->prev = place;
            place->next = runner;
            place->prev = runner_prev;
            return;
        }

        runner= runner->next;

    }

    //insert to tail
    runner_prev->next = place;
    place->prev = runner_prev;
    place->next = nullptr;
}

static size_t check_max_free_space(){
    for (int i = MAX_ORDER; i >= 0 ; i--)
    {
        if(bins[i]){
            return MIN_BLOCK_SIZE*pow(2, i)-meta_data_size;
        }
    }
    return 0 ;
}

 bool malicious_attack(MallocMetadata* block){
    (block->sweet_cookie == cookie_recipe)? false : true;
 }

// -------------------- Better Malloc Implementation  -------------------- // 

void* smalloc(size_t size){

    if(size==0 || size>MAX_ALLOCATE)
    {
        return nullptr;
    }

    if(first_smalloc){
        if(!initialize_free_space()){
            return nullptr;
        }
    }

    if(size < MAX_BLOCK_SIZE){
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

    else{ // size >= MMAP_THRESHOLD
        
        void *big_block_addr = mmap(NULL, (size + meta_data_size), PROT_WRITE | PROT_READ, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        if (big_block_addr == (void *)-1)
        {
            return nullptr;
        }
        MallocMetadata* big_block = (MallocMetadata *)big_block_addr;

        if(malicious_attack(big_block)){
            exit(0xdeadbeef);
        }

        big_block->is_free = false;
        big_block->sweet_cookie = cookie_recipe;
        big_block->next = nullptr;
        big_block->prev = nullptr; 


        total_allocated_blocks++;
        total_allocated_bytes += size; 
        total_meta_data_bytes += meta_data_size;

        return big_block_addr+meta_data_size;
        
    }
}


void *scalloc(size_t num, size_t size) {

    size_t bytes = num * size;
    void *return_addr = smalloc(bytes);
    if(!return_addr){
        return nullptr;
    }

    memset(return_addr,0,bytes);
    return return_addr;

}

void sfree(void *p) {
    if (!p){
        return;
    }

    MallocMetadata* block_let_it_go = (MallocMetadata*)((uintptr_t)p - (uintptr_t)meta_data_size);

    if(malicious_attack(block_let_it_go)){
        exit(0xdeadbeef);
    }

    if(block_let_it_go->is_free){
        return;
    }

    if (block_let_it_go->size >= MAX_BLOCK_SIZE)
    {
        total_allocated_blocks --;
        total_allocated_bytes -= block_let_it_go->size;
        total_meta_data_bytes -= meta_data_size;

        munmap(block_let_it_go, block_let_it_go->size + meta_data_size);
        return;
    }
    else{// add free block(s) to “buddy memory”

        int order = log2((block_let_it_go->size + meta_data_size)/MIN_BLOCK_SIZE);
        insert_block_to_bin(block_let_it_go, order);
        block_let_it_go->is_free = true;
        
        total_free_blocks++;
  

        MallocMetadata *buddy = find_buddy(block_let_it_go);

        while(buddy && order <= MAX_ORDER-1){

            MallocMetadata *merged_block = merge_buddies(block_let_it_go, buddy);
            remove_block_from_bin(block_let_it_go, order);
            remove_block_from_bin(buddy, order);
            insert_block_to_bin(merged_block, order+1);

            merged_block->is_free = true;

            total_free_blocks--;
            total_allocated_blocks--;
            total_meta_data_bytes -= meta_data_size;
            
            block_let_it_go = merged_block;
            buddy = find_buddy(block_let_it_go);
            order++;

        }

    }
    
}

void *srealloc(void *oldp, size_t size) {
    return;
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
    return total_meta_data_bytes;
}

size_t _size_meta_data()
{
    return meta_data_size;
}