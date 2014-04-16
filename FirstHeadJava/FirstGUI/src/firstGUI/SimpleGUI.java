package firstGUI;

import javax.swing.*;

import java.awt.event.*;

public class SimpleGUI implements ActionListener {
	
	JButton button;
	
	public static void main(String[] args) {
		SimpleGUI gui = new SimpleGUI();
		gui.go();
	}
	
	public void go() {
		JFrame frame = new JFrame();
		button = new JButton("click me");
		
		button.addActionListener(this);
		
		frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
		
		frame.getContentPane().add(button);
		
		frame.setSize(300,300);
		
		frame.setVisible(true);
	}
	
	//event is sent back from button to listener
	public void actionPerformed(ActionEvent event) {
		button.setText("I've been clicked");
	}

}
