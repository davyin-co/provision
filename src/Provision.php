<?php


namespace Aegir\Provision;

use Aegir\Provision\Console\Config;
use Aegir\Provision\Commands\ExampleCommands;

use League\Container\Container;
use League\Container\ContainerAwareInterface;
use League\Container\ContainerAwareTrait;
use Psr\Log\LoggerAwareInterface;
use Psr\Log\LoggerAwareTrait;
use Robo\Collection\CollectionBuilder;
use Robo\Common\ConfigAwareTrait;
use Robo\Common\IO;
use Robo\Contract\ConfigAwareInterface;
use Robo\Contract\IOAwareInterface;
use Robo\Robo;
use Robo\Runner as RoboRunner;
use Symfony\Component\Console\Input\InputInterface;
use Symfony\Component\Console\Output\OutputInterface;

class Provision implements ConfigAwareInterface, ContainerAwareInterface, LoggerAwareInterface, IOAwareInterface {
    
    const APPLICATION_NAME = 'Aegir Provision';
    const VERSION = '4.x-dev';
    const REPOSITORY = 'aegir-project/provision';
    
    use ConfigAwareTrait;
    use ContainerAwareTrait;
    use LoggerAwareTrait;
    use IO;
    
    /**
     * @var \Robo\Runner
     */
    private $runner;
    
    /**
     * @var string[]
     */
    private $commands = [];
    
    /**
     * Provision constructor.
     *
     * @param \Aegir\Provision\Console\Config                        $config
     * @param \Symfony\Component\Console\Input\InputInterface|null   $input
     * @param \Symfony\Component\Console\Output\OutputInterface|null $output
     */
    public function __construct(
        Config $config,
        InputInterface $input = NULL,
        OutputInterface $output = NULL
    ) {
        $this->setConfig($config);
        $this->setInput($input);
        $this->setOutput($output);

        // Create Application.
        $application = new Application(self::APPLICATION_NAME, self::VERSION, $this);
//        $application->setConfig($consoleConfig);
        
        // Create and configure container.
        $container = Robo::createDefaultContainer($input, $output, $application, $config);
        $this->setContainer($container);
        $this->configureContainer($container);
        
        // Instantiate Robo Runner.
        $this->runner = new RoboRunner([
            ExampleCommands::class
        ]);
        
        $this->runner->setContainer($container);
        $this->runner->setSelfUpdateRepository(self::REPOSITORY);
    }
    
    public function run(InputInterface $input, OutputInterface $output) {
        $status_code = $this->runner->run($input, $output);
        
        return $status_code;
    }
    
    /**
     * Register the necessary classes for BLT.
     */
    public function configureContainer(Container $container) {
    
        // FROM https://github.com/acquia/blt :
        // We create our own builder so that non-command classes are able to
        // implement task methods, like taskExec(). Yes, there are now two builders
        // in the container. "collectionBuilder" used for the actual command that
        // was executed, and "builder" to be used with non-command classes.
        $tasks = new ProvisionTasks();
        $builder = new CollectionBuilder($tasks);
        $tasks->setBuilder($builder);
        $container->add('builder', $builder);
    
    }
    
    /**
     * Temporary helper to allow public access to output.
     *
     * @return \Symfony\Component\Console\Output\OutputInterface
     */
    public function getOutput()
    {
        return $this->output();
    }
}