
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

---- Uncomment the following library declaration if instantiating
---- any Xilinx primitives in this code.
library UNISIM;
use UNISIM.VComponents.all;

-- Compute (OneA + OneB) + Two.
entity adder3 is
  generic (regOneA : integer;
           regOneB : integer;
           regTwo : integer;
           regInt : integer;
           regOut : integer);
  port ( OneA : in  STD_LOGIC_VECTOR (31 downto 0);
         OneB : in  STD_LOGIC_VECTOR (31 downto 0);
         Two : in  STD_LOGIC_VECTOR (31 downto 0);
         Sum : out  STD_LOGIC_VECTOR (31 downto 0);
         Clk : std_logic);
end adder3;

architecture Behavioral of adder3 is
  subtype word48_t is std_logic_vector (47 downto 0);
  signal OneA54 : std_logic_vector (53 downto 0);
  signal OneB48 : word48_t;
  signal Two54 : std_logic_vector (53 downto 0);

  signal OneSum : word48_t;
  signal Sum48 : word48_t;

  function en (r : integer) return std_logic is
  begin
    if r = 0 then
      return '0';
    else
      return '1';
    end if;
  end function;    

begin
  OneA54 <= "000000" & x"0000" & OneA;
  OneB48 <= x"0000" & OneB;
  Two54 <= "000000" & x"0000" & Two;
  Sum <= Sum48 (31 downto 0);

  AddOne : DSP48A
    generic map (
      A0REG => regOneA,
      A1REG => 0,
      B0REG => regOneA,
      B1REG => 0,
      CARRYINREG => 0,
      CARRYINSEL => "OPMODE5",
      CREG => regOneB,
      DREG => regOneA,
      MREG => 0, -- Don't care, multiplier not in use.
      OPMODEREG => 0, -- Constant anyway.
      PREG => regInt, -- Register output.
      RSTTYPE => "SYNC" -- Don't use it anyway.
      )
    port map (
      BCOUT=> open,
      CARRYOUT=> open,
      P=>open, -- We use PCOUT.
      PCOUT=> OneSum,

      A=> OneA54 (35 downto 18),
      B=> OneA54 (17 downto 0),
      C=> OneB48,
      CARRYIN=> '0',
      CEA=> en(regOneA),
      CEB=> en(regOneA),
      CEC=> en(regOneB),
      CECARRYIN=>'0', -- Carry in reg not used.
      CED=> en(regOneA),
      CEM=> '0',
      CEOPMODE=> '0',
      CEP=> en(regInt),
      CLK=> Clk,
      D=> OneA54 (53 downto 36),
      OPMODE=> "00011111", -- add/add / carry0/no-pre-add / C port/DBA
      PCIN=> x"000000000000",
      RSTA=> '0',
      RSTB=> '0',
      RSTC=> '0',
      RSTCARRYIN=> '0',
      RSTD=> '0',
      RSTM=> '0',
      RSTP=> '0',
      RSTOPMODE=> '0'
      );

  AddTwo : DSP48A
    generic map (
      A0REG => regTwo,
      A1REG => 0,
      B0REG => regTwo,
      B1REG => 0,
      CARRYINREG => 0,
      CARRYINSEL => "OPMODE5",
      CREG => 0, -- C not used
      DREG => regTwo,
      MREG => 0, -- Don't care, multiplier not in use.
      OPMODEREG => 0, -- Constant anyway.
      PREG => regOut, -- Register output.
      RSTTYPE => "SYNC" -- Don't use it anyway.
      )
    port map (
      BCOUT=> open,
      CARRYOUT=> open,
      P=> Sum48,
      PCOUT=> open,

      A=> Two54 (35 downto 18),
      B=> Two54 (17 downto 0),
      C=> x"000000000000",
      CARRYIN=> '0',
      CEA=> en(regTwo),
      CEB=> en(regTwo),
      CEC=> '0',
      CECARRYIN=>'0', -- Carry in reg not used.
      CED=> en(regTwo),
      CEM=> '0',
      CEOPMODE=> '0',
      CEP=> en(regOut),
      CLK=> Clk,
      D=> Two54 (53 downto 36),
      OPMODE=> "00010111", -- add/add / carry0/no-pre-add / PCin / DBA
      PCIN=> OneSum,
      RSTA=> '0',
      RSTB=> '0',
      RSTC=> '0',
      RSTCARRYIN=> '0',
      RSTD=> '0',
      RSTM=> '0',
      RSTP=> '0',
      RSTOPMODE=> '0'
      );

end Behavioral;
