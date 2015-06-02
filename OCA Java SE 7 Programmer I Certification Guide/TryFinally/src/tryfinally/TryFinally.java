package tryfinally;

class TryFinally {
	int tryAgain() 
	{
		int a = 10;
		try {
			++a;
		}
		finally {
			a++;
		}
		return a;
	}
	
	public static void main(String args[]) {
		System.out.println(new TryFinally().tryAgain());
	}
}
