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
    MallocMetadata* head = nullptr;
    MallocMetadata* tail = nullptr;

    public:
    List():head(nullptr),tail(nullptr){}
    void insert_block(size_t size);
};

void List::insert_block(size_t size)
{
    if(!head)
    {
        MallocMetadata* block_data = &MallocMetadata(size);
    }
}





List* List_pointer; 


void* smalloc(size_t size)
{
    if(size==0 || size>MAX_ALLOCATE)
    {
        return NULL;
    }


}