package linkedQueue;

public class LinkedQueue<E> implements Queue<E> {
	
	private Node<E> head = new Node<E>();// dummy node
	
	private int size;
	
	public void add(E element) 
	{
		head.prev = head.prev.next = new Node<E>(element, head.prev, head);
		++size;
	}
	
	public E element()
	{
		if (size == 0) 
		{
			throw new java.util.EmptyStackException();
		}
		
		return head.next.element; // front of queue // next <--> prev
	}
	
	public boolean isEmpty() 
	{
		return (size == 0);
	}
	
	public E remove() 
	{
		if (size == 0) 
		{
			throw new java.util.EmptyStackException();
		}
		
		E element = head.next.element; // next <--> prev
		head.next = head.next.next; // next <--> prev
		head.next.prev = head; // next <--> prev
		--size;
		return element;
	}
	
	public int size()
	{
		return size;
	}
	
	private static class Node<E> 
	{
		E element;
		Node<E> prev;
		Node<E> next;
		
		Node()
		{
			this.prev = this.next = this;
		}
		
		Node(E element, Node<E> prev, Node<E> next) 
		{
			this.element = element;
			this.prev = prev;
			this.next = next;
		}
	}
	/*
	public String toString()
	{
		Node<E> cursor = head.next;
		String ret = "";
		for (int i = 0; i < size; i++)
		{
			if(i != 0)
			{
				ret = ret + " ";
			}
			
			ret = ret + cursor.element;
			cursor = cursor.next;
		}
		return ret;
	}
	*/
	//
	public String toString()
	{
		Node<E> cursor = head.prev;
		String ret = "";
		for (int i = 0; i < size; i++)
		{
			if(i != 0)
			{
				ret = ret + " ";
			}
			
			ret = ret + cursor.element;
			cursor = cursor.prev;
		}
		return ret;
	}
	//
}
