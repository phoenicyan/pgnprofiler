#include <comutil.h>
#include <string.h>
#include <assert.h>

#include "threadalloc.h"

#include "malloc.h"

#ifdef REDEFINE_DEFAULT_NEW_OPERATOR

void* operator new[]( size_t size )
{
	return thread_alloc(size);
}

void* operator new(size_t size) 
{ 
    return thread_alloc(size);
}

void operator delete(void* addr) { 
    thread_free(addr);
}

void operator delete[]( void * p )
{
	thread_free(p);
}

#endif


#define N_CHAINS          5
#define MAX_BLOCK_SIZE    128
#define BLOCK_ALIGNMENT   8
#define PAGE_SIZE         4080
#define PENDING_THRESHOLD 100
 
// For Win32 only: specify period of checking for dead threads in alloc method
#define CHECK_FOR_DEAD_THREADS_FREQUENCY 100

static int block_chain[17] = { 
    0, 0, 0, /* 16 bytes */
    1, 1,    /* 32 bytes */
    2, 2,    /* 48 bytes */
    3, 3,    /* 64 bytes */
    4, 4, 4, 4, 4, 4, 4, 4 /* 128 bytes */
};

static int chain_block_size[N_CHAINS] = { 
    16, 32, 48, 64, 128
};

class thread_alloc_dir;

#define NOMALLOC

#ifdef MALLOC

#define ALLOC(x)		malloc(x)
#define FREE(x)			free(x)
#define REALLOC(x, y)	realloc(x, y)
#define HEAPMIN			_heapmin()

#else

#define ALLOC(x)		CoTaskMemAlloc(x)
#define FREE(x)			CoTaskMemFree(x)
#define REALLOC(x, y)	CoTaskMemRealloc(x, y)
#define HEAPMIN			__noop

#endif


#ifdef _WIN32

#include <windows.h>

class thread_context { 
  public:
    static void  set(void* obj) 
	{
		TlsSetValue(tls_index, obj);
    }
    static void* get() 
	{ 
        return TlsGetValue(tls_index);
    }
    
	thread_context() 
	{ 
		tls_index = TlsAlloc();
    }

    ~thread_context() 
	{ 
		TlsFree(tls_index);
    }    
  private:
    static int tls_index;    
};

int thread_context::tls_index;

class critical_section {
    CRITICAL_SECTION cs;
  public:
    critical_section() {
		InitializeCriticalSection(&cs);
    }
    ~critical_section() {
		DeleteCriticalSection(&cs);
    }
    void enter() {
		EnterCriticalSection(&cs);
    }
    void leave() {
		LeaveCriticalSection(&cs);
    }
};

#pragma init_seg(lib)

static critical_section  global_cs;
static thread_alloc_dir* thread_list;
static int               check_for_dead_threads_count;

#else

#include <pthread.h>

static void cleanup_thread_data(void* data);

class thread_context { 
  public:
    static void  set(void* obj) {
	pthread_setspecific(key, obj);
    }
    static void* get() { 
        return pthread_getspecific(key);    
    }
    thread_context() { 
	pthread_key_create(&key, cleanup_thread_data);
    }

    ~thread_context() { 
	pthread_key_delete(key);
    }    
  private:
    static pthread_key_t key;
};

pthread_key_t thread_context::key;

class critical_section {
  public:
    critical_section() {
	pthread_mutex_init(&mutex, NULL);
    }
    ~critical_section() {
	pthread_mutex_destroy(&mutex);
    }
    void enter() {
	pthread_mutex_lock(&mutex); 
    }
    void leave() {
	pthread_mutex_unlock(&mutex);
    }
  protected:
    pthread_mutex_t mutex;
};

#endif


struct alloc_page { 
    alloc_page*       next;
    thread_alloc_dir* dir;
    int               n_allocated_blocks;
    char              data[PAGE_SIZE];
};


struct alloc_block_header { 
    size_t       size;
    alloc_page*  page;
};

struct alloc_block : alloc_block_header { 
    alloc_block* next_free;
};
	

class thread_alloc_dir { 
  public:
    alloc_block*      chain[N_CHAINS];
    alloc_block*      pending_chain;
    alloc_page*       pages;
    int               n_allocated_objects;
    int               n_pending_requests;
    int               terminated;
    critical_section  cs;
#ifdef _WIN32
    HANDLE            thread;
    thread_alloc_dir* next;
#endif

    void* operator new(size_t size) { 
		return ALLOC(size);
    }

    void operator delete(void* p) { 
		FREE(p);
    }

    void process_pending_requests()
    {
		if (n_pending_requests > PENDING_THRESHOLD) 
		{ 
			cs.enter();
			alloc_block* blk = pending_chain;
			pending_chain = NULL;
			n_pending_requests = 0;
			cs.leave();
			while (blk != NULL) 
			{ 
				alloc_block* next = blk->next_free;
				int i = block_chain[blk->size / BLOCK_ALIGNMENT];
				blk->page->n_allocated_blocks -= 1;
				n_allocated_objects -= 1;
				blk->next_free = chain[i];
				chain[i] = blk;
				blk = next;
			}
		}
    }

    thread_alloc_dir() 
	{ 
		terminated = false;
		pending_chain = NULL;
		pages = NULL;
		memset(chain, 0, sizeof(chain));
		n_allocated_objects = 0;
		n_pending_requests = 0;
    }

    void cleanup();
};

static void cleanup_thread_data(void* data)
{
    ((thread_alloc_dir*)data)->cleanup();
}



void thread_alloc_dir::cleanup()
{
    cs.enter();
    while (true) { 
        for (alloc_block* blk = pending_chain; blk != NULL; blk = blk->next_free) 
		{ 
            blk->page->n_allocated_blocks -= 1;
            n_allocated_objects -= 1;
        }
        pending_chain = NULL;
        cs.leave();
        alloc_page *pg, *next, **ppg = &pages;
        for (pg = pages; pg != NULL; pg = next) 
		{ 
            next = pg->next;
            if (pg->n_allocated_blocks == 0) 
			{ 	    
                *ppg = next;
				FREE(pg);
            } else 
			{ 
                ppg = &pg->next;
            }
        }
        *ppg = NULL;
        cs.enter();
        if (n_allocated_objects == 0) 
		{ 
            delete this;
            return;
        } else { 
            if (pending_chain == NULL) 
			{ 
                terminated = true;
                break;
            }
        }
    }
    cs.leave();
}


static thread_context initializer;


void* thread_alloc(size_t size)
{
    size = ((size + BLOCK_ALIGNMENT-1) & ~(BLOCK_ALIGNMENT-1)) + sizeof(alloc_block_header);
    alloc_block* blk;
    if (size > MAX_BLOCK_SIZE) 
	{
		blk = (alloc_block*)ALLOC(size);
    } else 
	{ 
		thread_alloc_dir* dir = (thread_alloc_dir*)thread_context::get();
		if (dir == NULL) 
		{ 
			dir = new thread_alloc_dir();
			thread_context::set(dir);
	#ifdef _WIN32
			global_cs.enter();
			dir->next = thread_list;
			thread_list = dir;
			int succeed = DuplicateHandle(GetCurrentProcess(), 
						GetCurrentThread(),
						GetCurrentProcess(),  
						&dir->thread,
						0,
						FALSE,
						DUPLICATE_SAME_ACCESS);
			assert(succeed);
			global_cs.leave();
	#endif
		} else 
		{
			dir->process_pending_requests();
		}

		int i = block_chain[size / BLOCK_ALIGNMENT];
		blk = dir->chain[i];
		if (blk == NULL) 
		{ 
	#ifdef _WIN32
			if (++check_for_dead_threads_count == CHECK_FOR_DEAD_THREADS_FREQUENCY) 
			{
				global_cs.enter();
				check_for_dead_threads_count = 0;
				thread_alloc_dir *dp, **dpp = &thread_list; 
				while ((dp = *dpp) != NULL) 
				{ 
					if (dp != dir) 
					{ 
						DWORD status = 0;
						int succeed = GetExitCodeThread(dp->thread, &status);
						if (!succeed || status != STILL_ACTIVE) 
						{ 			
							*dpp = dp->next;
							CloseHandle(dp->thread);
							cleanup_thread_data(dp);
							continue;
						}
					}
					dpp = &dp->next;
				}
				global_cs.leave();
			}
	#endif	    
			alloc_page* pg = (alloc_page*)ALLOC(sizeof(alloc_page));
			pg->dir = dir;
			pg->next = dir->pages;
			pg->n_allocated_blocks = 0;
			dir->pages = pg;
			size_t block_size = chain_block_size[i];
			size_t n_blocks = PAGE_SIZE / block_size - 2;
			dir->chain[i] = blk = (alloc_block*)(pg->data + block_size);
			do 
			{
				alloc_block* next = (alloc_block*)((char*)blk + block_size);
				blk->page = pg;
				blk->next_free = next;
				blk = next;
			} while (--n_blocks != 0);
		    
			blk->page = pg;
			blk->next_free = NULL;
			blk = (alloc_block*)pg->data;
			blk->page = pg;
		} else 
		{ 
			dir->chain[i] = blk->next_free;
		}
		blk->page->n_allocated_blocks += 1;
		dir->n_allocated_objects += 1;
	}
    blk->size = size;
    return (alloc_block_header*)blk + 1;
}
    
void  thread_free(void* addr)
{
	if (!addr)
		return;
    alloc_block* blk = (alloc_block*)((alloc_block_header*)addr - 1);
    if (blk->size > MAX_BLOCK_SIZE) 
	{ 
		FREE(blk);
    } else { 
	alloc_page* page = blk->page;
	thread_alloc_dir* current = (thread_alloc_dir*)thread_context::get();
	thread_alloc_dir* allocator = page->dir;

	if (allocator != current) { 
	    allocator->cs.enter();
	    if (allocator->terminated) { 
		if (--page->n_allocated_blocks == 0) { 
			FREE(page);
		}
		if (--allocator->n_allocated_objects == 0) { 
		    delete allocator;
		    return;
		}
	    } else { 
		blk->next_free = allocator->pending_chain;
		allocator->pending_chain = blk;
		allocator->n_pending_requests += 1;
	    }
	    allocator->cs.leave();
	} else { 
	    int i = block_chain[blk->size / BLOCK_ALIGNMENT ];
	    page->n_allocated_blocks -= 1;
	    allocator->n_allocated_objects -= 1;
	    blk->next_free = allocator->chain[i];
	    allocator->chain[i] = blk;
	}
    }
}

void* thread_realloc(void* addr, size_t size)
{
    if (addr == NULL) { 
        return thread_alloc(size);
    }
    alloc_block_header* blk = (alloc_block_header*)addr - 1;
    size_t new_size = ((size + BLOCK_ALIGNMENT-1) & ~(BLOCK_ALIGNMENT-1)) + sizeof(alloc_block_header); 
    if (blk->size >= new_size) { 
        return addr;
    }
    if (blk->size > MAX_BLOCK_SIZE) { 
		blk = (alloc_block_header*)REALLOC(blk, new_size);
        blk->size = new_size;
        return (alloc_block_header*)blk + 1;
    } else { 
	void* new_addr = thread_alloc(size);    
	memcpy(new_addr, addr, blk->size - sizeof(alloc_block_header));
	thread_free(addr);
	return new_addr;
    }
}

	
	
