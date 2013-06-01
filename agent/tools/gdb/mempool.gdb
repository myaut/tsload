set $MPFRAGSIZE = 256
set $MPHEAPUNIT	= 4

set $FRAG_UNALLOCATED = -1

define dump_mempool
	set $_num_frags = mp_segment_size / $MPFRAGSIZE
	set $_i = 0
	
	printf "%d fragments found\n", $_num_frags
	
	while $_i < $_num_frags
		set $_frag = mp_frags[$_i]
		
		if $_frag.fd_type < 0
			set $_i = $_i + 1
		else
			set $_start = mp_segment + $_i * $MPFRAGSIZE
			set $_end = $_start + $_frag.fd_size * $MPFRAGSIZE 
		
			printf "FRAG [%p:%p] type: %d\n", $_start, $_end, $_frag.fd_type
		
			set $_i = $_i + $_frag.fd_size
		end
	end
end

define dump_heap_bst
	set $_hh = (mp_heap_header_t*) $arg0
	
	printf "FRAG @%p %d\n", $_hh, $_hh->hh_size
	
	if $_hh->hh_left != 0
		set $_left = $arg0 + $_hh->hh_left * $MPHEAPUNIT
		dump_heap_bst $_left
	end
	
	if $_hh->hh_right != 0
		set $_right = $arg0 + $_hh->hh_right * $MPHEAPUNIT
		dump_heap_bst $_right
	end
end

define dump_heap_page
	set $_page = (mp_heap_page_t*) $arg0
	
	printf "HEAP PAGE @%p\n", $_page
	printf "Free units: %u\n", $_page->hp_free
	printf "Free frags: %d\n", $_page->hp_free_count
	printf "Allocated frags: %d\n", $_page->hp_allocated_count
	
	if $_page->hp_root_free != 0
		dump_heap_bst $_page->hp_root_free
	end
end