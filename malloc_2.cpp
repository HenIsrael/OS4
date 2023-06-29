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
    size_t get_num_free_blocks();
    size_t get_num_free_bytes();
    size_t get_num_allocated_blocks();
    size_t get_num_allocated_bytes();
    size_t get_num_metadata_bytes();
    size_t get_size_metadata();

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

// -------------------- End Of Blocks List functions Implementation --------------------  //


// global pointer to the blocks list
List* List_pointer = (List *) sbrk(sizeof(List));;

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
    // TODO : add mplementation 

}

void sfree(void *p) {
    // TODO : add mplementation

}

void *srealloc(void *oldp, size_t size) {
    // TODO : add mplementation
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


