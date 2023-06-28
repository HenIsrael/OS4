#include <unistd.h>

#define MAX_ALLOCATE 100000000

struct MallocMetadata {
 size_t size;
 bool is_free;
 MallocMetadata* next;
 MallocMetadata* prev;
 void* start_address;

 public:
 MallocMetadata(size_t size): size(size),is_free(false),next(nullptr)
        ,prev(nullptr),start_address(nullptr){}
};

struct List {
    MallocMetadata* head;
    MallocMetadata* tail;

    public:
    List():head(nullptr),tail(nullptr){}
    void* insert_block(size_t size);
};

void* List::insert_block(size_t size)
{
    if(!head)
    {
        MallocMetadata block_data = MallocMetadata(size);
        head = &block_data;
        tail = head;
        if(sbrk(sizeof(MallocMetadata))== (void*)-1)
        {
            return NULL;
        }
        block_data.start_address = sbrk(size);
        if (block_data.start_address == (void*)-1)
        {
            return NULL;
        }
    }
    else
    {
        MallocMetadata* runner = head;
        while (runner != tail)
        {
            if(runner->is_free && runner->size>=size)
            {
                runner->is_free = false;
                return runner->start_address;
            }
            runner = runner->next;
        }
        if (runner == tail)
        {
            MallocMetadata block_data = MallocMetadata(size);
            if(sbrk(sizeof(MallocMetadata))== (void*)-1)
            {
                return NULL;
            }
            block_data.start_address = sbrk(size);
            if (block_data.start_address == (void*)-1)
            {
                return NULL;
            }
            block_data.prev = tail;
            tail->next = &block_data;
            tail = &block_data;
            return block_data.start_address;
        }
    }
}





List* List_pointer; 


void* smalloc(size_t size)
{
    if(size==0 || size>MAX_ALLOCATE)
    {
        return NULL;
    }
    return List_pointer->insert_block(size);
}