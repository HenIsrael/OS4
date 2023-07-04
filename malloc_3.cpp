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
    int orderi_free_count;
    for (int order = 0; order < MAX_ORDER+1; order++){

        MallocMetadata *current = bins[order];
        if(malicious_attack(current)){
            exit(0xdeadbeef);
        }

        orderi_free_count = 0;

        while (current){

            orderi_free_count ++;
            current = current->next;

        } 
        
        total_free_bytes_with_meta += orderi_free_count*MIN_BLOCK_SIZE*pow(2,order);
        //std::cout << "order : " << order << "count : " << orderi_free_count << std::endl;
    }

    //std::cout << "total free : " << total_free_blocks << std::endl;
    //std::cout << "total_free_bytes_with_meta : " << total_free_bytes_with_meta << std::endl;
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
    
    void *addr = sbrk((intptr_t)FREE_SPACE_CHUNK);//((intptr_t)FREE_SPACE_CHUNK);
    if(addr == (void*)(-1)){
        return nullptr;
    }

    // insert first block
    
    MallocMetadata *first = (MallocMetadata*)(uintptr_t)addr;
    first->prev = nullptr;
    first->next = nullptr;
    first->is_free = true;
    first->size = MAX_BLOCK_SIZE - meta_data_size;
    first->sweet_cookie = cookie_recipe;
    bins[10] = first;

    //std::cout << 0 << std::endl;
    //std::cout << "new_block : " << first  << std::endl;
    //std::cout << "prev      : " << first->prev << std::endl;
    //std::cout << "next      : " << first->next << std::endl;

    MallocMetadata *curr = first;
    

    for(int i=1; i<INITIAL_BLOCKS_NUM; i++){

        MallocMetadata *new_block = (MallocMetadata*)((uintptr_t)addr + MAX_BLOCK_SIZE*i);

        new_block->prev = curr;
        new_block->next = nullptr;
        curr->next = new_block;

        new_block->is_free = true;
        new_block->size = MAX_BLOCK_SIZE - meta_data_size;
        new_block->sweet_cookie = cookie_recipe;

        curr = new_block;


        // if (i == INITIAL_BLOCKS_NUM -1){ // special case for last block
        //     new_block->next = nullptr;
        //     size_t prev_offset = MAX_BLOCK_SIZE*(INITIAL_BLOCKS_NUM-2);
        //     new_block->prev = (MallocMetadata*)((uintptr_t)addr + prev_offset);
        // }

        // else if (i==0){ // special case for first block
        //     new_block->prev = nullptr;
        //     size_t next_offset = MAX_BLOCK_SIZE;
        //     new_block->next = (MallocMetadata*)((uintptr_t)addr + next_offset);
        //     bins[10] = new_block;
        // }

        // else{ // all other blocks
        //     size_t prev_offset = MAX_BLOCK_SIZE*(i-1);
        //     size_t next_offset = MAX_BLOCK_SIZE*(i+1);

        //     new_block->prev = (MallocMetadata*)((uintptr_t)addr + prev_offset);
        //     new_block->next = (MallocMetadata*)((uintptr_t)addr + next_offset);
            
        // }

        //std::cout << i << std::endl;
        //std::cout << "new_block : " << new_block  << std::endl;
        //std::cout << "prev      : " << new_block->prev << std::endl;
        //std::cout << "next      : " << new_block->next << std::endl;

    }


    total_free_blocks += INITIAL_BLOCKS_NUM;
    total_allocated_blocks += INITIAL_BLOCKS_NUM;
    total_allocated_bytes += FREE_SPACE_CHUNK - (INITIAL_BLOCKS_NUM*meta_data_size);
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
    //std::cout << "num of splits in split blocks func : " << num_of_splits << std::endl;

    if (num_of_splits == 0){

        // we don't need to split
        std::cout << "we dont need to split : " << num_of_splits << std::endl;
        MallocMetadata* block_to_remove = remove_block_from_bin(min_order);
        std::cout << "remove OK!!!!!!!!!!!!!!!!!!!!!!! : " << std::endl;
        block_to_remove->is_free = false;

        total_free_blocks--;
        //block_to_remove->cookie?

        std::cout << "block_to_remove : " << block_to_remove << std::endl;
        return (void*)block_to_remove;
    }
    else{
        // need to 
        //std::cout << "need to split : " << num_of_splits << std::endl;
        for (int i = order; i > min_order; i--){
            //std::cout << "before remove block from bin : "  << std::endl;
            void * addr_to_split = (void*)remove_block_from_bin(i);
            //std::cout << "after remove block from bin : "  << std::endl;
            MallocMetadata* buddy1 =  (MallocMetadata*)addr_to_split;

            if(malicious_attack(buddy1)){
                exit(0xdeadbeef);
            }

            uintptr_t buddy2_start_addr =  (uintptr_t)buddy1 + MIN_BLOCK_SIZE*pow(2,i-1);
            MallocMetadata* buddy2 = (MallocMetadata*)buddy2_start_addr;
            buddy2->sweet_cookie = cookie_recipe;
            //std::cout << "buddy 2 : "  <<buddy2<< std::endl;

            if(malicious_attack(buddy2)){
                exit(0xdeadbeef);
            }

            buddy1->size = MIN_BLOCK_SIZE*pow(2,i-1) - meta_data_size;
            buddy2->size = MIN_BLOCK_SIZE*pow(2,i-1) - meta_data_size;

            //std::cout << "before insert buddys : "  << std::endl;

            insert_block_to_bin(buddy1, i-1);
            insert_block_to_bin(buddy2, i-1);

            //std::cout << "after  insert buddys : "  << std::endl;

            total_meta_data_bytes += meta_data_size;
            total_free_blocks++; // we remove one but added two buddies
            total_allocated_bytes-=meta_data_size;
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
                //std::cout << "before " << order <<" : " << order<< " split blocks"  << std::endl;
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
    //std::cout << "adrress of removal block : "<< tmp << std::endl;
    return tmp;
}

static void remove_block_from_bin(MallocMetadata* block, int order){

    MallocMetadata *curr = bins[order];

    if(malicious_attack(curr)){
        exit(0xdeadbeef);
    }
    while(!curr->next){
        // block to remove in the middle
        if(curr == block){
            block->prev->next = block->next;
            block->next->prev = block->prev;
            block->next = nullptr;
            block->prev = nullptr;
            return;
        }

        curr = curr->next; 
    }
         
    if (curr == bins[order]){
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

    if(runner && malicious_attack(runner)){
        exit(0xdeadbeef);
    }
    if (!runner)
    {
        //std::cout << "insert   : if runner is nullptr " <<std::endl;
        bins[order]= place;
        bins[order]->prev = nullptr;
        bins[order]->next = nullptr;
        return;
    }
    
    //MallocMetadata* runner_prev;
    //std::cout << "insert   : entering to while loop  " <<std::endl;
    while (runner->next)
    {
        //runner_prev = runner->prev;

        // if(malicious_attack(runner_prev)){
        //     exit(0xdeadbeef);
        // }

        if((uintptr_t)place < (uintptr_t)runner)
        {
            if(runner == bins[order])
            {
                //case in head
                //std::cout << "insert   : case in head " <<std::endl;
                place->next = runner;
                place->prev = nullptr;
                runner->prev = place;
                bins[order] = place;
                return;
            }

            //case there is prev
            //std::cout << "insert   : case in prev " <<std::endl; 
            MallocMetadata* tmp = runner->prev;
            runner->prev = place;
            tmp->next = place;
            place->prev = tmp;
            place->next = runner;
            return;
        }

        runner= runner->next;

    }

    //insert to tail
    //std::cout << "insert   : case in tail " <<std::endl;
    runner->next = place;
    place->prev = runner;
    place->next = nullptr;
    return;
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
    return (block->sweet_cookie == cookie_recipe)? false : true;
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

            //std::cout << "before find minimak space with size : " << size  << std::endl;

            void* place = find_minimal_space(size);
            std::cout << place << std::endl;
            return (void *)((uintptr_t)place+meta_data_size);
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
            //std::cout << " big block mmap failed "<< std::endl;
            return nullptr;
        }
        MallocMetadata* big_block = (MallocMetadata *)big_block_addr;
        big_block->sweet_cookie = cookie_recipe;

        if(malicious_attack(big_block)){
            exit(0xdeadbeef);
        }

        big_block->is_free = false;
        big_block->next = nullptr;
        big_block->prev = nullptr; 
        big_block->size = size;


        total_allocated_blocks++;
        total_allocated_bytes += size; 
        total_meta_data_bytes += meta_data_size;

        //std::cout << " malloc mmapsucced "<< std::endl;
        return (void *)((uintptr_t)big_block_addr+meta_data_size);
        
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
    std::cout << "im in free"  << std::endl ; 
    if (!p){
        return;
    }

    MallocMetadata* block_let_it_go = (MallocMetadata*)((uintptr_t)p - (uintptr_t)meta_data_size);
    //std::cout << "block let it go address is :" << block_let_it_go << std::endl ; 

    std::cout << "before cookie :"  << std::endl ; 
    if(malicious_attack(block_let_it_go)){
        exit(0xdeadbeef);
    }
    std::cout << "after cookie :"  << std::endl ; 

    if(block_let_it_go->is_free){
        return;
    }

    if (block_let_it_go->size >= MAX_BLOCK_SIZE)
    {
        //std::cout << "currently in if maxblocksize " << std::endl ; 
        total_allocated_blocks --;
        total_allocated_bytes -= block_let_it_go->size;
        total_meta_data_bytes -= meta_data_size;

        munmap(block_let_it_go , block_let_it_go->size + meta_data_size); 
        return;
    }
    else{// add free block(s) to “buddy memory”

        std::cout << "in else in sfree "<< std::endl;
        int order = log2((block_let_it_go->size + meta_data_size)/MIN_BLOCK_SIZE);
        std::cout << "order in log2 : "<< order << std::endl;
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
            total_allocated_bytes+=meta_data_size;
            total_meta_data_bytes -= meta_data_size;
            
            block_let_it_go = merged_block;
            buddy = find_buddy(block_let_it_go);
            order++;

        }

    }
    
}

void *srealloc(void *oldp, size_t size) {
    return (void *)-1;
}

size_t _num_free_blocks()
{
    return total_free_blocks;
}

size_t _num_free_bytes()
{
    if(first_smalloc)
    {
        return 0;
    }
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