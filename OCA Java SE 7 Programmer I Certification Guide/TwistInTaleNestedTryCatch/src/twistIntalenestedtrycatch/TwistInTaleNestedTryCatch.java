package twistIntalenestedtrycatch;

import java.io.*;

public class TwistInTaleNestedTryCatch {
	
	static FileInputStream players, coach;
	
	public static void main(String args[]) 
	{
		try 
		{
			players = new FileInputStream("players.txt");
			System.out.println("players.txt found");
			
			try 
			{
				coach.close();
			}
			catch (IOException e) 
			{
				System.out.println("coach.txt not found");
			}
		}
		catch (FileNotFoundException fnfe) 
		{
			System.out.println("players.txt not found");
		}
		catch (NullPointerException ne) 
		{
			System.out.println("NullPointerException");
		}
	}
}