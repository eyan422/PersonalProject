package firstThread;

public class ThreadTester {
	public static void main (String[] args) {
		Runnable threadJob = new FirstThread();
		
		Thread myThread = new Thread(threadJob);
		myThread .start();
		System.out.println("back in main");
	}
}
