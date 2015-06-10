package label;

public class Label {
	public static void main(String args[])
	{
		String[] programmers = {"Outer", "Inner"};
		outer:
		for (String outer : programmers) 
		{
			for (String inner : programmers) 
			{
				if (inner.equals("Inner"))
					break outer;
				System.out.println(inner + ":");
			}
		}
		
		String[] programmers1 = {"Paul", "Shreya", "Selvan", "Harry"};
		outer:
		for (String name1 : programmers1) 
		{
			for (String name : programmers1) 
			{
				if (name.equals("Shreya"))
					continue outer;
				System.out.println(name);
			}
		}
	}
}
