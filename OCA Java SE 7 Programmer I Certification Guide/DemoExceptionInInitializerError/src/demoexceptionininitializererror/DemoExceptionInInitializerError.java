package demoexceptionininitializererror;

public class DemoExceptionInInitializerError {
	static 
	{
		int num = Integer.parseInt("sd", 16);
	}
	
	public static void main(String args[])
	{
		
	}
}
