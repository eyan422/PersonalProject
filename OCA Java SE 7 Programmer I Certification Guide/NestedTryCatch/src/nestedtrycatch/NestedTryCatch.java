package nestedtrycatch;

import java.io.*;

public class NestedTryCatch {
	FileInputStream players, coach;
	
	public void myMethod() {
		try 
		{
			players = new FileInputStream("players.txt");
			
			try 
			{
				coach = new FileInputStream("coach.txt");
				//.. rest of the code
			}
			catch (FileNotFoundException e) 
			{
				System.out.println("coach.txt not found");
			}
			//.. rest of the code
		}
		catch (FileNotFoundException fnfe) 
		{
			System.out.println("players.txt not found");
		}
		finally 
		{
			try 
			{
				System.out.println("close files");
				players.close();
				coach.close();
			}
			catch (IOException ioe) 
			{
				System.out.println("finally");
				System.out.println(ioe);
			}
		}
	}
	
	public static void main(String args[])
	{
		NestedTryCatch ntc = new NestedTryCatch();
		ntc.myMethod();
	}
}