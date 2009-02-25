
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

---- Uncomment the following library declaration if instantiating
---- any Xilinx primitives in this code.
--library UNISIM;
--use UNISIM.VComponents.all;

entity top is
  Port (EClk : in  STD_LOGIC;
        Din : in  STD_LOGIC_VECTOR (31 downto 0);
        Dout : out  STD_LOGIC_VECTOR (31 downto 0));
end top;

architecture Behavioral of top is
  component md5
    port (xxIN : in std_logic_vector (31 downto 0);
          Aout : out std_logic_vector (31 downto 0);
          Bout : out std_logic_vector (31 downto 0);
          Cout : out std_logic_vector (31 downto 0);
          Dout : out std_logic_vector (31 downto 0);
          Clk : in std_logic;
          phaseA : in std_logic;
          phaseB : in std_logic);
  end component;
  component clock
    port ( CLKIN_IN        : in    std_logic; 
           CLKIN_IBUFG_OUT : out   std_logic; 
           CLK0_OUT        : out   std_logic; 
           CLK2X_OUT       : out   std_logic; 
           CLK180_OUT      : out   std_logic);
  end component;
  signal Clk : std_logic;
  signal phaseA : std_logic;
  signal phaseB : std_logic;
begin
  main: md5 port map (xxIN => Din, Aout => Dout,
                      Bout => open, Cout => open, Dout => open,
                      Clk => Clk, phaseA => phaseA, phaseB => phaseB);
  ck: clock port map (clkin_in => EClk,
                      clk2x_out => Clk,
                      clk0_out => phaseA, clk180_out => phaseB);

end Behavioral;

