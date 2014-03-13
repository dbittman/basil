; doesn't handle branching yet
start:
	get
	dup
	push 0
	cmp
	push do_push
	push 1
	swap
	br
	
	push 0
	swap
	push start
	push 1
	br
	
	do_push:
		pop
		get
		push start
		push 1
		br
