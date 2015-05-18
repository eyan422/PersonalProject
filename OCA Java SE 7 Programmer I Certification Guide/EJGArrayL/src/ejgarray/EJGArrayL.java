package ejgarray;

import java.util.*;
class EJGArrayL {
	public static void main(String args[]) {
		ArrayList<String> seasons = new ArrayList<String>();
		seasons.add(0,"Spring");
		
		seasons.add(1, "Spring"); seasons.add(2, "Summer");
		seasons.add(3, "Autumn"); seasons.add(4, "Winter");
		
		seasons.remove(2);
		for (String s : seasons)
			System.out.print(s + ", ");
	}
}
