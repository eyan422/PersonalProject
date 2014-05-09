package arrayQueue;

//import java.util.ArrayDeque;
//import java.util.Queue;

public class Testing {
	public static void main(String[] args) 
	{
		ArrayQueue<String> queue = new ArrayQueue<String>();
		
		queue.add("GB");
		queue.add("DE");
		queue.add("FR");
		queue.add("ES");
		
		System.out.println(queue);
		
		System.out.println("queue.element(): " + queue.element());
		System.out.println("queue.remove(): " + queue.remove());
		System.out.println(queue.toString());
		
		System.out.println("queue.remove(): " + queue.remove());
		System.out.println(queue);
		
		System.out.println("queue.add(\"IE\"): ");
		queue.add("IE");
		System.out.println(queue);
		
		System.out.println("queue.remove(): " + queue.remove());
		System.out.println(queue);
	}
}
