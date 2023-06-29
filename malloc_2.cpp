#include <unistd.h>

#define MAX_ALLOCATE 100000000

struct MallocMetadata {
    size_t size;
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;
    void* start_address;

    MallocMetadata(size_t size): size(size),is_free(false),next(nullptr)
        ,prev(nullptr),start_address(nullptr){}
};

struct List {
    MallocMetadata* head;
    MallocMetadata* tail;

    List():head(nullptr),tail(nullptr){}
    void* insert_block(size_t size);
    void free_space(void* address);
    size_t get_num_free_blocks();
    size_t get_num_free_bytes();
    size_t get_num_allocated_blocks();
    size_t get_num_allocated_bytes();
    size_t get_num_metadata_bytes();
    size_t get_size_metadata();
    size_t get_block_size(void* address);

};

// -------------------- Blocks List functions Implementation --------------------  //

void* List::insert_block(size_t size)
{
    /**
     * first call to allocate new block, i.e. the list is empty
     * allocate and insert the first block to the list
    */
    if(!head)
    {
        MallocMetadata *first_block = (MallocMetadata *) sbrk(sizeof(MallocMetadata));

        if(first_block==(void*)-1){
            return nullptr;
        }
    
        void* address = sbrk(size);

        if(address==(void*)-1){
            return nullptr;
        }

        first_block->size = size;
        first_block->is_free = false;
        first_block->start_address = address;
        first_block->next = nullptr;
        first_block->prev = nullptr;

        head = first_block;
        tail = first_block;
        
        return address;
    }

    /**
     * if you get here the list is not empty
     * first, trying to find free block to use
     * in case no free block to use, allocate new block and insert to the end of the list
    */

    else
    {
        MallocMetadata *runner = head;
        while (runner)
        {
            if(runner->is_free && runner->size>=size)
            {
                runner->is_free = false;
                return runner->start_address;
            }

            runner = runner->next;
        }

        // no found of big enough free block. must allocate new one

        MallocMetadata *block = (MallocMetadata *) sbrk(sizeof(MallocMetadata));

        if(block==(void*)-1){
            return nullptr;
        }

        void* address = sbrk(size);

        if(address==(void*)-1){
            return nullptr;
        }

        block->size = size;
        block->is_free = false;
        block->start_address = address;

        block->prev = tail;
        tail = block;
        tail->prev->next = tail;
        tail->next = nullptr;

        return address;
        
    }
}

void List::free_space(void* address){

    if(!head){
        return;
    }

    MallocMetadata *runner = head;
    while(runner){
        if(runner->start_address==address && runner->is_free==false){
            runner->is_free = true;
            return;
        }

        runner = runner->next;
    }

    return;
}

size_t List::get_num_free_blocks(){

}
size_t List::get_num_free_bytes(){

}
size_t List::get_num_allocated_blocks(){

}
size_t List::get_num_allocated_bytes(){

}
size_t List::get_num_metadata_bytes(){

}
size_t List::get_size_metadata(){

}

size_t List::get_block_size(void* address){

    if (!head){
        return 0;
    }

    MallocMetadata *runner = head;
    while(runner){
        if(runner->start_address=address){
            return runner->size;
        }
        runner = runner->next;
    }

    return 0;
}

// -------------------- End Of Blocks List functions Implementation --------------------  //


// global pointer to the blocks list
List* List_pointer = (List *) sbrk(sizeof(List));

// -------------------- Basic Malloc functions --------------------  //


void* smalloc(size_t size)
{
    if(size==0 || size>MAX_ALLOCATE)
    {
        return nullptr;
    }

    return List_pointer->insert_block(size);
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

    List_pointer->free_space(p);

}


void *srealloc(void *oldp, size_t size) {

    if(size==0 || size>MAX_ALLOCATE)
    {
        return nullptr;
    }
    if(!oldp){
        return smalloc(size);
    }

    size_t block_size_in_addr = List_pointer->get_block_size(oldp);

    if(size <= block_size_in_addr){
        return oldp;
    }

    //need to allocate new block and copy the old metadata to the new

    void* new_block_addr = smalloc(size);
    if(!new_block_addr){
        return nullptr;
    }

    memmove(new_block_addr, oldp, block_size_in_addr);
    sfree(oldp);
    return new_block_addr;
    
}

size_t _num_free_blocks() {
    return List_pointer->get_num_free_blocks();
}

size_t _num_free_bytes() {
    return List_pointer->get_num_free_bytes();
}

size_t _num_allocated_blocks() {
    return List_pointer->get_num_allocated_blocks();
}

size_t _num_allocated_bytes() {
    return List_pointer->get_num_allocated_bytes();
}

size_t _num_meta_data_bytes() {
    return List_pointer->get_num_metadata_bytes();
}

size_t _size_meta_data() {
    return List_pointer->get_size_metadata();
}

// -------------------- End Of Basic Malloc functions --------------------  //


