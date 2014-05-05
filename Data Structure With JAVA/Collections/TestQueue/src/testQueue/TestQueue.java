package testQueue;

import java.util.*;

public class TestQueue {
	public static void main(String[] args) {
		Queue<String> queue = new LinkedQueue<String>();
		
		Collections.addAll(queue, "AR", "BO", "CO", "EC");
		System.out.println(queue);
		
		String firstOut = queue.remove();
		
		System.out.println(queue);
		System.out.printf("Removed %s%n", firstOut);
		
		queue.add("PE");
		System.out.println(queue);
		
		String secondOut = queue.remove();
		System.out.println(queue);
		
		System.out.printf("Removed %s%n", secondOut);
	}
}

class LinkedQueue<E> extends AbstractQueue<E> implements Queue<E> {
	private List<E> list = new LinkedList<E>();

	public Iterator<E> iterator() {
		return list.iterator();
	}
	 
	public boolean offer(E e) {
		if (e == null) {
			return false;
		} 
		else 
		{
			list.add(e);
			return true;
		}
	}

	public E peek() {
		return list.get(0);
	}

	public E poll() {
		if (list.isEmpty()) {
			return null;
		} 
		else 
		{
			return list.remove(0);
		}
	 }

 	public int size() {
 		return list.size();
 	}
}
