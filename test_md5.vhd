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

architecture Behavioral of test_md5 is
   component md5 is
    Port (in0 : in  std_logic_vector (31 downto 0);
        in1 : in  std_logic_vector (31 downto 0);
        in2 : in  std_logic_vector (31 downto 0);
        hit : out std_logic;
        Aout : out std_logic_vector (31 downto 0);
        Bout : out std_logic_vector (31 downto 0);
        Cout : out std_logic_vector (31 downto 0);
        Dout : out std_logic_vector (31 downto 0);
        Clk : in std_logic);
   end component;
   signal in0 : std_logic_vector (31 downto 0);
   signal in1 : std_logic_vector (31 downto 0);
   signal in2 : std_logic_vector (31 downto 0);
   signal Aout : std_logic_vector (31 downto 0);
   signal Bout : std_logic_vector (31 downto 0);
   signal Cout : std_logic_vector (31 downto 0);
   signal Dout : std_logic_vector (31 downto 0);
   signal Clk : std_logic;
begin
   UUT : md5 port map (in0=>in0,in1=>in1,in2=>in2,
   Aout=>Aout,Bout=>Bout,Cout=>Cout,Dout=>Dout,Clk=>Clk);

   process
   begin
      in0 <= x"00000000";
      in1 <= x"00000000";
      in2 <= x"00000000";
      Clk <= '0';
      -- First, clock 100 times so that outputs settle.
      for i in 1 to 100 loop
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

