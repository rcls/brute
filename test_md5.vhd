----------------------------------------------------------------------------------
-- Company: 
-- Engineer: 
-- 
-- Create Date:    11:38:27 03/21/2009 
-- Design Name: 
-- Module Name:    test_md5 - Behavioral 
-- Project Name: 
-- Target Devices: 
-- Tool versions: 
-- Description: 
--
-- Dependencies: 
--
-- Revision: 
-- Revision 0.01 - File Created
-- Additional Comments: 
--
----------------------------------------------------------------------------------
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

---- Uncomment the following library declaration if instantiating
---- any Xilinx primitives in this code.
--library UNISIM;
--use UNISIM.VComponents.all;

entity test_md5 is
end test_md5;

library work;
use work.defs.all;

architecture Behavioral of test_md5 is
   component md5 is
    Port (in0 : in  word;
        in1 : in  word;
        in2 : in  word;
        Aout : out word;
        Bout : out word;
        Cout : out word;
        Dout : out word;
		  		  bmon : out dataset (0 to 64);

        Clk : in std_logic);
   end component;
   signal in0 : word;
   signal in1 : word;
   signal in2 : word;
   signal Aout : word;
   signal Bout : word;
   signal Cout : word;
   signal Dout : word;
	signal bmon : dataset (0 to 64);

   signal Clk : std_logic;
begin
   UUT : md5 port map (in0=>in0,in1=>in1,in2=>in2,
   Aout=>Aout,Bout=>Bout,Cout=>Cout,Dout=>Dout,Clk=>Clk,bmon=>bmon);

   process
   begin
      in0 <= x"00000000";
      in1 <= x"00000000";
      in2 <= x"00000000";
      Clk <= '0';
      -- First, clock 200 times so that outputs settle.
      for i in 1 to 200 loop
         Clk <= '0';
         wait for 0.5 us;
         Clk <= '1';
         wait for 0.5 us;
      end loop;
      -- Set some inputs and clock once.
      in0 <= x"e040a4f0";
      in1 <= x"7d4a91b5";
      in2 <= x"694f8475";
      Clk <= '0';
      wait for 0.5 us;
      Clk <= '1';
      wait for 0.5 us;

      -- Now clock in zeros 100 times again.
      in0 <= x"00000000";
      in1 <= x"00000000";
      in2 <= x"00000000";
      --x3 <= x"00000000";
      Clk <= '0';
      -- First, clock 100 times so that outputs settle.
      for i in 1 to 1000 loop
         Clk <= '0';
         wait for 0.5 us;
         Clk <= '1';
         wait for 0.5 us;
      end loop;
   end process;

end Behavioral;

