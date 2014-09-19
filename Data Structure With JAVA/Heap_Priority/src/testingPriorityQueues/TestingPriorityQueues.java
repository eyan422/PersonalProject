package testingPriorityQueues;

import java.util.PriorityQueue;

public class TestingPriorityQueues {
	public static void main(String[] args){
		PriorityQueue<String> pq = new PriorityQueue<String>();
		pq.add("FR");
		pq.add("DE");
		pq.add("GB");
		pq.add("IT");
		pq.add("ES");
		while(!pq.isEmpty())
		{
			System.out.printf("%s ",pq.remove());
		}
	}
}
