#include <unistd.h>
#include <cstring>
#include <cstdint>
#include <sys/mman.h>
#include <cmath>
#include <cstdio>
#include <iostream>

#include <vector>


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
static void print_building();
//////////////////////////////////////////////////////////////////

// Implementation part
static size_t total_free_bytes(){

    size_t total_free_bytes_with_meta = 0;
    int orderi_free_count;
    for (int order = 0; order < MAX_ORDER+1; order++){

        if(!bins[order]){
            continue;
        }

        MallocMetadata *current = bins[order];
        if(malicious_attack(current)){
            exit(0xdeadbeef);
        }

        orderi_free_count = 0;

        while(current){

            orderi_free_count ++;
            current = current->next;

        } 
        
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

    }


    total_free_blocks += INITIAL_BLOCKS_NUM;
    total_allocated_blocks += INITIAL_BLOCKS_NUM;
    total_allocated_bytes += FREE_SPACE_CHUNK - (INITIAL_BLOCKS_NUM*meta_data_size);
    total_meta_data_bytes += INITIAL_BLOCKS_NUM*meta_data_size;

    first_smalloc = false; 
    print_building();
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

        return (void*)block_to_remove;
    }
    else{

        for (int i = order; i > min_order; i--){

            void * addr_to_split = (void*)remove_block_from_bin(i);
            MallocMetadata* buddy1 =  (MallocMetadata*)addr_to_split;

            if(malicious_attack(buddy1)){
                exit(0xdeadbeef);
            }

            uintptr_t buddy2_start_addr =  (uintptr_t)buddy1 + MIN_BLOCK_SIZE*pow(2,i-1);
            MallocMetadata* buddy2 = (MallocMetadata*)buddy2_start_addr;
            buddy2->sweet_cookie = cookie_recipe;

            if(malicious_attack(buddy2)){
                exit(0xdeadbeef);
            }

            buddy1->size = MIN_BLOCK_SIZE*pow(2,i-1) - meta_data_size;
            buddy2->size = MIN_BLOCK_SIZE*pow(2,i-1) - meta_data_size;

            insert_block_to_bin(buddy1, i-1);
            insert_block_to_bin(buddy2, i-1);


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

    void *potential_buddy_addr = (void*)((uintptr_t)block ^ (block->size+meta_data_size));
    std::cout << "potential buddy addr" << potential_buddy_addr << std::endl;

    // check existence for buddy
    for(int order=0; order<MAX_ORDER+1; order++){

        if(!bins[order]){
            continue;
        }

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
    // big_monster->next = nullptr;
    // big_monster->prev = nullptr;

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
    if (!bins[order]){
        printf("trying to delete null value!!!!");
        return;
    }
    
    MallocMetadata *curr = bins[order];


    
    if(malicious_attack(curr)){
        exit(0xdeadbeef);
    }

    // case head
    if(block == curr){
        if (!block->next)
        // case head and lonly
        {

            bins[order] = nullptr;
            block->next = nullptr;
            block->prev = nullptr;
            return;
        }

        else{
            // head have friends

            bins[order] = block->next;
            bins[order]->prev = nullptr;
            block->next = nullptr;
            block->prev = nullptr;
            return;
        }
        
    }

    while(curr){

        if(curr == block){
            //case tail
            
            if(!curr->next){

                block->prev->next = nullptr;

                block->next = nullptr;
                block->prev = nullptr;
                return;
            }

            else{
                //case middle

                curr->prev->next = curr->next;
                curr->next->prev = curr->prev;
                block->next = nullptr;
                block->prev = nullptr;
                return;
            }

        }

        curr = curr->next;
        
   
    }

    std::cout << "OPS!!!"   << std::endl;
    return;

}

static void insert_block_to_bin(MallocMetadata* place, int order){
    MallocMetadata* runner = bins[order];

    if(runner && malicious_attack(runner)){
        exit(0xdeadbeef);
    }
    if (!runner)
    {
        //if list in that order is empty
        bins[order]= place;
        bins[order]->prev = nullptr;
        bins[order]->next = nullptr;
        return;
    }

    
    // case head lonley
    if(!bins[order]->next){
        if((uintptr_t)place < (uintptr_t)bins[order]){

            // place will be in the head
            MallocMetadata* tmp = bins[order];

            bins[order] = place;
            bins[order]->prev = nullptr;
            bins[order]->next = tmp;
            tmp->prev = bins[order];
            tmp->next = nullptr;
            return;
        }

        else{
            // place will be in the tail
            bins[order]->next = place;
            place->prev = bins[order];
            place->next = nullptr;
            return;
        }
        
    }
    
    while (runner)
    {
        if(!runner->next && (uintptr_t)place > (uintptr_t)runner)
        {
            //insert place  to the tail
            runner->next = place;
            place->prev = runner;
            place->next = nullptr;
            return;

        }

        if((uintptr_t)place < (uintptr_t)runner)
        {
            //insert place to the middle
            MallocMetadata *tmp = runner->prev;
            place->next = runner;
            runner->prev = place;
            tmp->next = place;
            place->prev = tmp;
            return;
        }
        
        runner= runner->next;

    }

    printf("Opppps");
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

 static void print_building(){

    std::vector<int> building = {};

    for(int order=0; order<MAX_ORDER+1; order++){

        if(!bins[order]){
            building.push_back(0);
            continue;
        }
        

        MallocMetadata *current = bins[order];

        if(malicious_attack(current)){
            exit(0xdeadbeef);
        }

        int orderi_free_count = 0;

        while(current){

            orderi_free_count ++;
            current = current->next;

        }

        building.push_back(orderi_free_count);
        

    }


    for (const auto& element : building) {
        std::cout << element << " ";
    }

    std::cout << std::endl;
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
            print_building();
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

        print_building();
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
 
    if (!p){
        return;
    }

    MallocMetadata* block_let_it_go = (MallocMetadata*)((uintptr_t)p - (uintptr_t)meta_data_size);
    std::cout << "now we trying to free block: " << block_let_it_go << std::endl;

    if(malicious_attack(block_let_it_go)){
        exit(0xdeadbeef);
    }


    if(block_let_it_go->is_free){
        std::cout << "trying to free block that already free" << std::endl;
        return;
    }

    if (block_let_it_go->size >= MAX_BLOCK_SIZE)
    {
 
        total_allocated_blocks --;
        total_allocated_bytes -= block_let_it_go->size;
        total_meta_data_bytes -= meta_data_size;

        munmap(block_let_it_go , block_let_it_go->size + meta_data_size); 
        return;
    }
    else{// add free block(s) to “buddy memory”

        int order = log2((block_let_it_go->size + meta_data_size)/MIN_BLOCK_SIZE);
        std::cout << "add the block to order: " << order << std::endl;
        insert_block_to_bin(block_let_it_go, order);
        block_let_it_go->is_free = true;
        std::cout << "after insert block let it go: "<< std::endl;
        print_building();

        total_free_blocks++;
  

        MallocMetadata *buddy = find_buddy(block_let_it_go);


        while(buddy && order <= MAX_ORDER-1){

            std::cout << "we want merge buddies : " << std::endl;
            std::cout << "buddy #1 : " << block_let_it_go << std::endl;
            std::cout << "buddy #2 : " << buddy << std::endl;
            std::cout << "in order : " << order <<  std::endl;
            

            MallocMetadata *merged_block = merge_buddies(block_let_it_go, buddy);
            
            std::cout << "removing buddies and insert merged block : "  <<  std::endl;
            print_building();

            remove_block_from_bin(block_let_it_go, order);
            block_let_it_go->is_free = false;
            std::cout << "remove buddy #1 OK!!! : " << block_let_it_go << std::endl;
            print_building();

            remove_block_from_bin(buddy, order);
            buddy->is_free = false;
            std::cout << "remove buddy #2 OK!!! : " << buddy << std::endl;
            print_building();

            insert_block_to_bin(merged_block, order+1);
            merged_block->is_free = true;
            std::cout << "after insert merged block: "<< std::endl;
            print_building();
           
            total_free_blocks--;
            total_allocated_blocks--;
            total_allocated_bytes+=meta_data_size;
            total_meta_data_bytes -= meta_data_size;
            
            std::cout << "merged block: "<< merged_block << "size = " << merged_block->size<<  std::endl;
            MallocMetadata *buddy_of_merged_block = bins[1]->next; 
            std::cout << "buddy of merged block: "<< buddy_of_merged_block <<  std::endl;
            buddy = find_buddy(merged_block);
            std::cout << "budyyyy: "<< buddy <<  std::endl;

            block_let_it_go = merged_block;
            
            
            order++;
            
        }

    }

    print_building();

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