package arrayStack;

import java.util.ArrayDeque;
import java.util.Deque;

public class ArrayStack<E> implements Stack<E> {
	private E[] elements;
	private int size;
	private static final int INITIAL_CAPACITY = 100;
	
	@SuppressWarnings("unchecked")
	public ArrayStack() {
		elements = (E[]) new Object[INITIAL_CAPACITY];
	}
	
	@SuppressWarnings("unchecked")
	public ArrayStack(int capacity) {
		elements = (E[]) new Object[capacity];
	}
	
	public boolean isEmpty() {
		return (size == 0);
	}
	
	public E peek() {
		if (size == 0) {
			throw new java.util.EmptyStackException();
		}
		return elements[size-1]; // top of stack
	}
	
	public E pop() {
		if (size == 0) {
			throw new java.util.EmptyStackException();
	}
		
	E element = elements[--size];
	elements[size] = null;
	return element;
	}
	public void push(E element) {
		if (size == elements.length) {
			resize();
		}
		elements[size++] = element;
	}
	
	public int size() {
		return size;
	}
	
	@SuppressWarnings("unchecked")
	private void resize() {
		assert size == elements.length;
		Object[] a = new Object[2*size];
		System.arraycopy(elements, 0, a, 0, size);
		elements = (E[])a;
	}
	
	public static void main(String[] args) {
		Deque<String> stack = new ArrayDeque<String>();
		stack.push("GB");
		stack.push("DE");
		stack.push("FR");
		stack.push("ES");
		System.out.println(stack);
		System.out.println("stack.peek(): " + stack.peek());
		System.out.println("stack.pop(): " + stack.pop());
		System.out.println(stack);
		System.out.println("stack.pop(): " + stack.pop());
		System.out.println(stack);
		System.out.println("stack.push(\"IE\"): ");
		stack.push("IE");
		System.out.println(stack);
	}
}
