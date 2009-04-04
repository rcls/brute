library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

---- Uncomment the following library declaration if instantiating
---- any Xilinx primitives in this code.
library UNISIM;
use UNISIM.VComponents.all;

library work;
use work.defs.all;

entity control is
  port (Clk_125MHz : in std_logic;
        LED : out byte_t);
end control;

architecture Behavioral of control is

  component BSCAN_SPARTAN3A
    port (TCK : out STD_ULOGIC;
          TMS : out STD_ULOGIC;
          CAPTURE : out STD_ULOGIC;
          DRCK1 : out STD_ULOGIC;
          DRCK2 : out STD_ULOGIC;
          RESET : out STD_ULOGIC;
          SEL1 : out STD_ULOGIC;
          SEL2 : out STD_ULOGIC;
          SHIFT : out STD_ULOGIC;
          TDI : out STD_ULOGIC;
          UPDATE : out STD_ULOGIC;
          TDO1 : in STD_ULOGIC;
          TDO2 : in STD_ULOGIC);
  end component;

  component Clock
    port (CLKIN_IN : in STD_LOGIC; CLKFX_OUT : out STD_LOGIC);
  end component;

  signal jtag_tck : std_logic;
  signal jtag_tms : std_logic;
  signal jtag_capture : std_logic;
  signal jtag_drck1 : std_logic;
  signal jtag_drck2 : std_logic;
  signal jtag_sel1 : std_logic;
  signal jtag_sel2 : std_logic;
  signal jtag_shift : std_logic;
  signal jtag_tdi : std_logic;
  signal jtag_update : std_logic;
  signal jtag_tdo1 : std_logic;
  signal jtag_tdo2 : std_logic;

  -- We use a 48 bit cycle counter.
  subtype word144_t is std_logic_vector (143 downto 0);

  component md5 is
    port (input : in word96_t;
          output : out word128_t;
          Clk : in std_logic);
  end component;

  component counter is
    port (count : out word48_t;
          match : in word48_t;
          hit : out std_logic;
          clk : in std_logic);
  end component;

  signal Clk : std_logic;

  signal md5_next : word96_t;           -- 3 input words to md5.
  signal md5_output : word128_t;        -- 4 output words from md5.

  -- 152 bit shift register attached to user1.
  -- 8 bit opcode / 48 bit clock / 96 bits data  or
  -- 8 bit opcode / 8 bit address.
  -- Opcode is bitmask:
  -- bit 0 : read result + 8 bit address
  -- bit 1 : load md5 - 48 bit clock count + 96 bits data.
  -- bit 2 : sample md5 - 48 bit clock count
  signal command : std_logic_vector (151 downto 0);

  signal command_valid : std_logic := '0';

  alias command_op_readram : std_logic is command (144);
  alias command_op_load : std_logic is command (145);
  alias command_op_sample : std_logic is command (146);

  -- Detect rising edge with "01" and falling by "10"; we're crossing
  -- clock domains.
  signal command_edge : std_logic_vector (1 downto 0) := "00";

  -- The dual ported hit ram.
  type hit_ram_t is array (255 downto 0) of word144_t;
  signal hit_ram : hit_ram_t;
  -- Output buffer the bram needs.
  signal hit_ram_o : word144_t;
  -- The hit ram allocation counter.
  signal hit_idx : byte_t;
  -- Did we hit?
  signal hit : std_logic;

  -- The 48 bit global cycle counter.
  signal global_count : word48_t;
  signal global_count_latch : word48_t;
  signal global_count_match : std_logic; -- Does global count match command?
  signal load_match : std_logic; -- Buffered load command hit.
  signal sample_match0 : std_logic; -- Sample command hit.
  signal sample_match : std_logic; -- Buffered sample command hit.

begin

  BSCAN_SPARTAN3_inst : BSCAN_SPARTAN3A
    port map (
      TCK => jtag_tck,
      TMS => jtag_tms,
      CAPTURE => jtag_capture, -- CAPTURE output from TAP controller
      DRCK1 => jtag_drck1,     -- Data register output for USER1 functions
      DRCK2 => jtag_drck2,        -- Data register output for USER2 functions
      --RESET => jtag_RESET,        -- Reset output from TAP controller
      SEL1 => jtag_sel1,          -- USER1 active output
      SEL2 => jtag_sel2,          -- USER2 active output
      SHIFT => jtag_shift,        -- SHIFT output from TAP controller
      TDI => jtag_tdi,            -- TDI output from TAP controller
      UPDATE => jtag_update,      -- UPDATE output from TAP controller
      TDO1 => jtag_tdo1,          -- Data input for USER1 function
      TDO2 => jtag_tdo2           -- Data input for USER2 function
      );

  clock_inst : clock port map (
    CLKFX_OUT => Clk,
    CLKIN_IN => Clk_125MHz);

  -- The outputs; the jtag unit seems to take care of latching on falling TCK.
  jtag_tdo1 <= command (0);

  process (jtag_tck)
  begin
    if jtag_tck'event and jtag_tck = '1' then
      if jtag_sel1 = '1' then
        if jtag_shift = '1' then
          -- Shift in the command.
          command <= jtag_tdi & command (151 downto 1);
        end if;
        if jtag_capture = '1' then
          command_valid <= '0';
          -- If the previous command read ram then grab the data out of the
          -- hit-ram.  Else grab the latched clock.
          if command_op_readram = '1' then
            command <= x"00" & hit_ram_o;
          else
            command <= x"00000000000000000000000000" & global_count_latch;
          end if;
        elsif jtag_update = '1' then
          command_valid <= '1';
        end if;
      end if;
    end if;
  end process;

  global_cnt : counter
    port map (count => global_count,
              match => command (143 downto 96),
              hit => global_count_match,
              Clk => Clk);

  m : md5 port map (input => md5_next, output => md5_output, Clk => Clk);

  -- Nice LEDs
  LED <= hit_idx;

  process (Clk)
  begin
    if Clk'event and Clk = '1' then
      -- Run the command_valid edge.  This serves both edge detection and
      -- as a guard against metastability.
      command_edge(0) <= command_valid;
      command_edge(1) <= command_edge(0);

      -- Write into the hit ram on hits.
      if hit = '1' then
        hit_ram (conv_integer(hit_idx)) <= global_count & next2 & next1 & next0;
        hit_idx <= hit_idx + 1;
      end if;

      -- Read the hit_ram.
      hit_ram_o <= hit_ram (conv_integer (command (143 downto 136)));

      -- On command_valid rising, latch the global counter.
      if command_edge = "01" then
        global_count_latch <= global_count;
      end if;

      -- Buffer the load-match, sample-match and hit.  The write takes place
      -- the cycle after the load mux, so be careful about that.
      load_match <= command_edge(1) and global_count_match and command_op_load;
      sample_match <= command_edge(1) and global_count_match and command_op_sample;
      if outA(23 downto 0) = x"000000" or sample_match = '1' then
        hit <= '1';
      else
        hit <= '0';
      end if;

      -- Calculate the next value to feed into the pipeline.
      if load_match = '1' then
        md5_next <= command (95 downto 0);
      else
        md5_next <= md5_output;
      end if;

    end if;
  end process;

end Behavioral;
