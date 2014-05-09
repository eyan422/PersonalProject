package convertingInfixtoPostfix;

import java.util.*;

public class ConvertingInfixtoPostfix {
	public static void main(String[] args) {
		
		Deque<String> stack = new ArrayDeque<String>();
		String line = new Scanner(System.in).nextLine();
		System.out.println(line);
		
		Scanner scanner = new Scanner(line);
		
		while (scanner.hasNext()) 
		{
			if (scanner.hasNextInt())
			{
				System.out.print(scanner.nextInt() + " ");
			} 
			else 
			{
				String str = scanner. next();
				//System.out.println("fya " + str);
				/*if (str.equals("-"))
				{
					System.out.println("push");
					stack.push(str);
				} 
				else*/
				if ( str.equals(")") ) 
				{
					//System.out.println("pop");
					System.out.print(stack.pop() + " ");
				}
				else
				{
					//System.out.println("push");
					stack.push(str);
				}
			}
		}
		
		while (!stack.isEmpty()) 
		{
			System.out.print(stack.pop() + " ");
		}
		System.out.println();
		
		scanner.close();
	}
}