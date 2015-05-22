package calculation;

public class Calculation {
	public static void main(String args[])
	{
		String result = "1";
		int score = 10;
		if ((score = score+10) == 100)
		result = "A";
		else if ((score = score+29) == 50)
		result = "B";
		else if ((score = score+200) == 10)
		result = "C";
		else
		result = "F";
		System.out.println(result + ":" + score);
	}
}
